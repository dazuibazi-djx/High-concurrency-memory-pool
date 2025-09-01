#include "common.h"
#include "PageCache.h"
PageCache PageCache::_sInst;  // 在 .cpp 中去定义这个变量


// 获得一个大块的span，使用了这个 span 就要将它放到哈希桶之中，方便后续的查找
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// 整体的逻辑为：去 k 页里面看一下有没有空闲的 span 
	// 就是两种情况有、没有，有直接返回，没有就是去看大的有没有有就切分，没有就向堆申请

	// 如果是申请大块内存, 使用巧妙的方法，都是使用 span
	if (k > NSPANLIST - 1)
	{
		void* ptr = SystemAlloc(k);
		//Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)ptr >> SHIFT_SIZE;
		span->_n = k;
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);
		return span;
	}
	if (!_spanlist[k].Empty())
	{
		Span* kSpan = _spanlist[k].PopFront();
		// 这个里面有 span 进行返回之前需要先把它都存放到哈希桶中
		for (PAGE_ID i = 0; i < kSpan->_n; i++)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}
		return kSpan;
	}

	// 说明没有先去下面去找找
	for (size_t i = k + 1; i < NSPANLIST; i++)
	{
		if (!_spanlist[i].Empty())
		{
			Span* nspan = _spanlist[i].PopFront();
			//Span* kspan = new Span();
			Span* kspan = _spanPool.New();
			// 进行拆分, kspan 进行赋值
			kspan->_pageId = nspan->_pageId;
			kspan->_n = k;
			// nspan 的 Id 是向后走 k 
			nspan->_pageId += k;
			nspan->_n -= k;
			// 然后将 nspan 插入到桶之中
			_spanlist[nspan->_n].PushFront(nspan);
			// 存储nSpan的首位页号跟nSpan映射，方便page cache回收内存时
			// 进行的合并查找
			/*_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;*/
			_idSpanMap.set(nspan->_pageId, nspan);
			_idSpanMap.set(nspan->_pageId + nspan->_n - 1, nspan);
			// 返回之前同样的需要将建立映射关系, 方便centercache的碎片内存
			for (PAGE_ID i = 0; i < kspan->_n; i++)
			{
				//_idSpanMap[kspan->_pageId + i] = kspan;
				_idSpanMap.set(kspan->_pageId + i, kspan);
			}
			//	将 k 这个内存进行返回
			return kspan;
		}
	}

	// 说明最大的都不能进行拆分，需要向堆进行申请空间
	// 开辟一个 128 的大块span
	//Span* bigspan = new Span();
	Span* bigspan = _spanPool.New();
	bigspan->_n = NSPANLIST - 1;
	void* ptr = SystemAlloc(NSPANLIST - 1);
	bigspan->_pageId = (PAGE_ID)ptr >> SHIFT_SIZE;
	_spanlist[bigspan->_n].PushFront(bigspan);
	// 使用递归调用
	return NewSpan(k);
}
Span* PageCache::MapObjectToSpan(void* obj)
{
	// 根据这个 obj， 找到 Pageid ，然后进行返回
	PAGE_ID id = (PAGE_ID)obj >> SHIFT_SIZE;
	// 保证线程安全，使用哈希的时候可能回改变结构，所以要加锁
	// std::unique_lock<std::mutex> lock(_pageMtx);

	/*auto it = _idSpanMap.find(id);
	if (it != _idSpanMap.end())
	{
		return it->second;
	}*/
	//else
	//{

	//	assert(false);
	//	// 使用条件断点
	//	/*if (it == _idSpanMap.end()) int x = 0;*/
	//	return nullptr;
	//}
	auto it = (Span*)_idSpanMap.get(id);
	assert(it != nullptr);
	return it;
}
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 给到了 span 之后通过他的 pageid（页的起始地址)
	// 循环的查看前面的以及后面的 span 是不是正在进行使用，使用多添加的一个变量 isUsed;
	// 大于 128 的span直接返回到堆中
	if (span->_n > NSPANLIST - 1)
	{
		void* ptr = (void*)(span->_pageId << SHIFT_SIZE);
		SystemFree(ptr);
		// 然后释放 span 
		_spanPool.Delete(span);
		return;
		return;
	}
	while (1)
	{
		// 向前合并
		PAGE_ID prevId = span->_pageId - 1;
		/*auto ret = _idSpanMap.find(prevId);
		if (ret == _idSpanMap.end()) break;*/
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr) break;
		Span* prevSpan = ret;
		if (prevSpan->_isUsed == true) break;
		if (prevSpan->_n + span->_n > NSPANLIST - 1) break;
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;
		// 将这个 prevSpan 进行删除以及释放
		_spanlist[prevSpan->_n].Earse(prevSpan);
		_spanPool.Delete(prevSpan);
		//delete prevSpan;
	}
	while (1)
	{
		// 向后合并
		PAGE_ID nextId = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(nextId);
		//if (ret == _idSpanMap.end()) break;
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr) break;
		Span* nextSpan = ret;
		if (nextSpan->_isUsed == true) break;
		if (nextSpan->_n + span->_n > NSPANLIST - 1) break;
		span->_n += nextSpan->_n;
		_spanlist[nextSpan->_n].Earse(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}
	//	将这个 span 插入到 page 这个桶之中，然后将前后都放入到哈希表里面
	_spanlist[span->_n].PushFront(span);
	span->_isUsed = false;
	//_idSpanMap[span->_pageId] = span;
	//_idSpanMap[span->_pageId + span->_n - 1] = span;

	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}