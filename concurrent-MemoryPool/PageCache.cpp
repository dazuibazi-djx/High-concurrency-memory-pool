#include "common.h"
#include "PageCache.h"
PageCache PageCache::_sInst;  // �� .cpp ��ȥ�����������


// ���һ������span��ʹ������� span ��Ҫ�����ŵ���ϣͰ֮�У���������Ĳ���
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// ������߼�Ϊ��ȥ k ҳ���濴һ����û�п��е� span 
	// ������������С�û�У���ֱ�ӷ��أ�û�о���ȥ�������û���о��з֣�û�о��������

	// ������������ڴ�, ʹ������ķ���������ʹ�� span
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
		// ��������� span ���з���֮ǰ��Ҫ�Ȱ�������ŵ���ϣͰ��
		for (PAGE_ID i = 0; i < kSpan->_n; i++)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}
		return kSpan;
	}

	// ˵��û����ȥ����ȥ����
	for (size_t i = k + 1; i < NSPANLIST; i++)
	{
		if (!_spanlist[i].Empty())
		{
			Span* nspan = _spanlist[i].PopFront();
			//Span* kspan = new Span();
			Span* kspan = _spanPool.New();
			// ���в��, kspan ���и�ֵ
			kspan->_pageId = nspan->_pageId;
			kspan->_n = k;
			// nspan �� Id ������� k 
			nspan->_pageId += k;
			nspan->_n -= k;
			// Ȼ�� nspan ���뵽Ͱ֮��
			_spanlist[nspan->_n].PushFront(nspan);
			// �洢nSpan����λҳ�Ÿ�nSpanӳ�䣬����page cache�����ڴ�ʱ
			// ���еĺϲ�����
			/*_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;*/
			_idSpanMap.set(nspan->_pageId, nspan);
			_idSpanMap.set(nspan->_pageId + nspan->_n - 1, nspan);
			// ����֮ǰͬ������Ҫ������ӳ���ϵ, ����centercache����Ƭ�ڴ�
			for (PAGE_ID i = 0; i < kspan->_n; i++)
			{
				//_idSpanMap[kspan->_pageId + i] = kspan;
				_idSpanMap.set(kspan->_pageId + i, kspan);
			}
			//	�� k ����ڴ���з���
			return kspan;
		}
	}

	// ˵�����Ķ����ܽ��в�֣���Ҫ��ѽ�������ռ�
	// ����һ�� 128 �Ĵ��span
	//Span* bigspan = new Span();
	Span* bigspan = _spanPool.New();
	bigspan->_n = NSPANLIST - 1;
	void* ptr = SystemAlloc(NSPANLIST - 1);
	bigspan->_pageId = (PAGE_ID)ptr >> SHIFT_SIZE;
	_spanlist[bigspan->_n].PushFront(bigspan);
	// ʹ�õݹ����
	return NewSpan(k);
}
Span* PageCache::MapObjectToSpan(void* obj)
{
	// ������� obj�� �ҵ� Pageid ��Ȼ����з���
	PAGE_ID id = (PAGE_ID)obj >> SHIFT_SIZE;
	// ��֤�̰߳�ȫ��ʹ�ù�ϣ��ʱ����ܻظı�ṹ������Ҫ����
	// std::unique_lock<std::mutex> lock(_pageMtx);

	/*auto it = _idSpanMap.find(id);
	if (it != _idSpanMap.end())
	{
		return it->second;
	}*/
	//else
	//{

	//	assert(false);
	//	// ʹ�������ϵ�
	//	/*if (it == _idSpanMap.end()) int x = 0;*/
	//	return nullptr;
	//}
	auto it = (Span*)_idSpanMap.get(id);
	assert(it != nullptr);
	return it;
}
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// ������ span ֮��ͨ������ pageid��ҳ����ʼ��ַ)
	// ѭ���Ĳ鿴ǰ����Լ������ span �ǲ������ڽ���ʹ�ã�ʹ�ö���ӵ�һ������ isUsed;
	// ���� 128 ��spanֱ�ӷ��ص�����
	if (span->_n > NSPANLIST - 1)
	{
		void* ptr = (void*)(span->_pageId << SHIFT_SIZE);
		SystemFree(ptr);
		// Ȼ���ͷ� span 
		_spanPool.Delete(span);
		return;
		return;
	}
	while (1)
	{
		// ��ǰ�ϲ�
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
		// ����� prevSpan ����ɾ���Լ��ͷ�
		_spanlist[prevSpan->_n].Earse(prevSpan);
		_spanPool.Delete(prevSpan);
		//delete prevSpan;
	}
	while (1)
	{
		// ���ϲ�
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
	//	����� span ���뵽 page ���Ͱ֮�У�Ȼ��ǰ�󶼷��뵽��ϣ������
	_spanlist[span->_n].PushFront(span);
	span->_isUsed = false;
	//_idSpanMap[span->_pageId] = span;
	//_idSpanMap[span->_pageId + span->_n - 1] = span;

	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}