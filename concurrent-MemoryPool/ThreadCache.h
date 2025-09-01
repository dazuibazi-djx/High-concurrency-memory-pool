#pragma once
#include "common.h"
//内存池第一层 ThreadCache
class ThreadCache
{
public:
	// 在Thread 当中有申请和释放内存对象这两个函数
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);
	// 从中心缓存获取对象
	void* FetchFromCenterCache(size_t index, size_t size);
	// 如果太长了就调用这个函数
	void ListTooLong(FreeList& list, size_t size);
private:
	//使用哈希桶的方式存放内存
	FreeList _freeLists[NFREELIST];
	
};
// 为了保证开辟的线程相互独立，使用 TLS 
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
