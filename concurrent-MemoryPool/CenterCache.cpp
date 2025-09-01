#include "CenterCache.h"
#include "common.h"
#include "PageCache.h"
CenterCache CenterCache::_sInst; // �� .h �������������ﶨ��
// �����Ļ����ȡһ�������Ķ����thread cache����󷵻ص��� CenterCahche �е�ʵ�ʸ���
size_t CenterCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	// ��� size ��Ӧ��Ͱ
	size_t index = SizeClass::Index(size);
	//	�����Ͱ����
	_spanLists[index]._mtx.lock();
	Span* span = GetOneSpan(_spanLists[index], size); // һ��ʼ�ȼ����һ�ȡ���ҵ�һ���ͷ���span
	assert(span);
	assert(span->freelist); // span ����ҵĶ���������
	// �� end �����
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
	//	����
	_spanLists[index]._mtx.unlock();
	return actualNum;
}
// �� list �л�ý�����ڴ��зֺõ�span�������ǲ�Ϊ�յ�span���ظ�Center
Span* CenterCache::GetOneSpan(SpanList& list, size_t size)
{
	// ��������һ��Ͱ��������Ҫ�����Ͱ������б���
	Span* it = list.Begin();
	while (it != list.End())
	{
		// ����Ǵ���ֱ�ӷ��� it
		if (it->freelist != nullptr)
		{
			return it;
		}
		else
		{
			it = it->next;
		}
		
	}
	// ��Ҫ����
	list._mtx.unlock();
	// �ߵ����˵��û������Ҫ�� Span�� �����Ҫ�� PageCahce ����Ҫ
	//	Ҫ������ʱ����Ҫ�� pagecache ���м���
	PageCache::GetInstance()->_pageMtx.lock();
	// NumMovePage ��������Ҫ�� size ��С�ҵ���Ӧ��ҳ��
	Span* getspan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	getspan->_isUsed = true;
	getspan->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// �������ҵĵ���һ��span�������span �����з�
	// �зֵĴ�СΪ size, ʹ�� span ����� _nҳ�ĸ������Լ� _pageID ����ȷ��
	// ȷ��һ�� start �Լ�һ�� byte �ҵ� end
	char* start = (char*)(getspan->_pageId << SHIFT_SIZE);
	size_t byte = getspan->_n << SHIFT_SIZE; // ��ʾ��� span ���е��ֽ���
	char* end = start + byte;
	// �Ѵ���ڴ��г�����������������
	// 1������һ������ȥ��ͷ������β��
	getspan->freelist = start;
	start += size;
	void* tail = getspan->freelist;

	// 2������������ѭ���Ľ���β��
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		start += size;
	}

	NextObj(tail) = nullptr;
	// �� span �ҵ����Ͱ���棬�ҹ�ȥ��ʱ����Ҫ��������ֹ�������߳̽��и���
	// ��Ҫ�Ƚ�����Ȼ���ټ���
	list._mtx.lock();
	list.PushFront(getspan);

	return getspan;
}

void CenterCache::ReleaseListToSpans(void* start, size_t size)
{
	// ��Ҫʹ�� start ���ڵ� �Ǹ� list �鲻ͬ��list �ŵ���ͬ��span�У������ size �涨��Ͱ
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		// ʹ�ù�ϣͰ���� list ��Ӧ�� span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		// �� start ����ͷ�嵽 span ��
		NextObj(start) = span->freelist;
		span->freelist = start;
		span->_useCount--;
		if (span->_useCount == 0)
		{
			// ���������Ĵ������ڴ棬���ݸ�pagechahe���ٺϲ���������ڴ�
			_spanLists[index].Earse(span);
			// ����һ�»��� pagecache �е�span
			span->freelist = nullptr;
			span->next = span->prev = nullptr;
			// �����»���ʱ���ǽ�Ͱ���⿪��Ȼ���� page ������
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