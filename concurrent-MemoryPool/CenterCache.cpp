#include "CenterCache.h"
#include "common.h"
#include "PageCache.h"
CenterCache CenterCache::_sInst; // 在 .h 中声明，在这里定义
// 从中心缓存获取一定数量的对象给thread cache，最后返回的是 CenterCahche 中的实际个数
size_t CenterCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	// 获得 size 对应的桶
	size_t index = SizeClass::Index(size);
	//	给这个桶上锁
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index], size); // 一开始先假设我获取是找到一个和法的span
	assert(span);
	assert(span->freelist); // span 下面挂的东西必须有
	// 让 end 向后走
	start = span->freelist;
	end = start;
	size_t actualNum = 1;
	size_t i = 0;
	while ((NextObj(end) != nullptr) && (i < (batchNum - 1)))
	{
		end = NextObj(end);
		actualNum++;
		i++;
	}
	span->freelist = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;
	//	解锁
	_spanLists[index]._mtx.unlock();
	return actualNum;
}
// 从 list 中获得将大块内存切分好的span，或者是不为空的span返回给Center
Span* CenterCache::GetOneSpan(SpanList& list, size_t size)
{
	// 给到了我一个桶，我们需要在这个桶里面进行遍历
	Span* it = list.Begin();
	while (it != list.End())
	{
		// 如果是存在直接返回 it
		if (it->freelist != nullptr)
		{
			return it;
		}
		else
		{
			it = it->next;
		}
		
	}
	// 需要解锁
	list._mtx.unlock();
	// 走到这个说明没有我想要的 Span， 因此需要向 PageCahce 中索要
	//	要东西的时候需要对 pagecache 进行加锁
	PageCache::GetInstance()->_pageMtx.lock();
	// NumMovePage 根据我需要的 size 大小找到对应的页数
	Span* getspan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	getspan->_isUsed = true;
	getspan->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// 接下来我的到了一个span，将这个span 进行切分
	// 切分的大小为 size, 使用 span 里面的 _n页的个数，以及 _pageID 进行确定
	// 确定一个 start 以及一个 byte 找到 end
	char* start = (char*)(getspan->_pageId << SHIFT_SIZE);
	size_t byte = getspan->_n << SHIFT_SIZE; // 表示这个 span 所有的字节数
	char* end = start + byte;
	// 把大块内存切成自由链表链接起来
	// 1、先切一块下来去做头，方便尾插
	getspan->freelist = start;
	start += size;
	void* tail = getspan->freelist;

	// 2、接下来就是循环的进行尾插
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		start += size;
	}

	NextObj(tail) = nullptr;
	// 将 span 挂到这个桶里面，挂过去的时候需要加锁，防止其他的线程进行干扰
	// 需要先解锁，然后再加锁
	list._mtx.lock();
	list.PushFront(getspan);

	return getspan;
}

void CenterCache::ReleaseListToSpans(void* start, size_t size)
{
	// 需要使用 start 所在的 那个 list 块不同的list 放到不同的span中，这个是 size 规定的桶
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		// 使用哈希桶查找 list 对应的 span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		// 将 start 进行头插到 span 中
		NextObj(start) = span->freelist;
		span->freelist = start;
		span->_useCount--;
		if (span->_useCount == 0)
		{
			// 将还回来的大块混乱内存，传递给pagechahe，再合并出更大的内存
			_spanLists[index].Earse(span);
			// 处理一下还到 pagecache 中的span
			span->freelist = nullptr;
			span->next = span->prev = nullptr;
			// 当向下还的时候是将桶锁解开，然后在 page 中上锁
			_spanLists[index]._mtx.unlock();
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();
			_spanLists[index]._mtx.lock();
		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();
}