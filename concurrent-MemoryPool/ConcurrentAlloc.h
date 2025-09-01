#pragma once
#include "common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "objectpool.h"
static void* ConcurrentAlloc(size_t size)
{
	//assert(size <= 256 * 1024);
	// ���һ��Ҫ����Ŀռ���� 256kb�� Ҳ����ʹ��pagecavhe
	if (size > MAX_BYTES)
	{
		// ����һ�¶���
		size_t alignSize = SizeClass::RoundUp(size);
		// �������ĸ�ҳͰ֮��
		size_t kpage = alignSize >> SHIFT_SIZE;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();
		void* ptr = (void*)(span->_pageId << SHIFT_SIZE);
		return ptr;
	}
	else
	{
		if (pTLSThreadCache == nullptr) pTLSThreadCache = new ThreadCache;
		//// ���в���һ�¶������߳�
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
		return pTLSThreadCache->Allocate(size);
	}
	
}
static void ConcurrentFree(void* ptr)
{
	// ͨ�� ptr �õ� span
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if(size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		// �� span ���ص�ҳ���浱��
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
	
}