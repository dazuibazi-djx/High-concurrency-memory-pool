#include "ThreadCache.h"
#include "CenterCache.h"
void* ThreadCache::Allocate(size_t size)
{
	// 要空间首先要对 size 所要的空间大小进行对齐，找对对齐大小，然后再去桶里面找，然后进行 pop 返回
	// 进行对齐的目的也在于最大限度的减少内碎片的问题
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	//开辟空间看看对应的 freelist 里面有没有如果有直接 pop， 没有就要想下一层去要
	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].pop();
	}
	else
	{
		// 传递桶的下标，以及对齐好的大小。
		return FetchFromCenterCache(index, alignSize);
	}
}
void* ThreadCache::FetchFromCenterCache(size_t index, size_t size)
{
	//	从 CenterCache 中获得一些划分好的内存块
	//	使用慢增长的算法
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));

	if (batchNum == _freeLists[index].MaxSize()) _freeLists[index].MaxSize()++;
	void* start = nullptr;
	void* end = nullptr;
	// 从 centercahche 中获得相应大小的小块内存，由 span 切好
	size_t actualNum = CenterCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		// 将 start 后面循环挂起来，然后返回 start 
		// 将第一个返回之后要把后面的 n - 1 个挂起来
		_freeLists[index].pushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
} 

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// 进行删除就是放回到自由链表里面
	size_t index = SizeClass::Index(size);

	_freeLists[index].push(ptr);
	//如果是一个这个链上面的个数大于一个可以申请的最大个数那么，就需要还给 CenterCache
	if (_freeLists[index].size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	// 保存这个 list 的开始与结束位置
	// 1.先从 list 里面解除掉，然后调用中心缓存的函数 ReleaseListToSpans
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	// 使用开始位置，以及个数直接调用我们的函数
	CenterCache::GetInstance()->ReleaseListToSpans(start, size);

}