#pragma once
#include "common.h"
class CenterCache
{
public:
	static CenterCache* GetInstance()
	{
		return &_sInst;
	}
	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 从中心缓存获取一定数量的对象给thread cache， 利用输出型参数的方式
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// 释放资源从thread 到 center 的spans中
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanLists[NFREELIST];	// 规定有 多个桶，然后里面的 Span 是一个定义的结构体，定义在common.h 中
//	使用单例模式，饿汉模式减少内存以及创建一个对象，在没有开始的时候就创建好对象
private:
	CenterCache()
	{}

	CenterCache(const CenterCache&) = delete;

	static CenterCache _sInst;
};