#pragma once
#include "common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "objectpool.h"
static void* ConcurrentAlloc(size_t size)
{
	//assert(size <= 256 * 1024);
	// 如果一次要申请的空间大于 256kb， 也可以使用pagecavhe
	if (size > MAX_BYTES)
	{
		// 进行一下对齐
		size_t alignSize = SizeClass::RoundUp(size);
		// 计算在哪个页桶之中
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
		//// 进行测试一下独立的线程
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
		return pTLSThreadCache->Allocate(size);
	}
	
}
static void ConcurrentFree(void* ptr)
{
	// 通过 ptr 得到 span
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if(size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		// 将 span 返回到页缓存当中
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
	
}