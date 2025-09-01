#pragma once
#include "common.h"
#include "PageMap.h"
#include "objectpool.h"
// 在 page 这个结构中是一个 128 页的span桶构成的一个 spanlist
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	// 将Page里面 span 给 Centercache, 需要直到是从哪个页里面寻找到span
	Span* NewSpan(size_t k);
	std::mutex _pageMtx;
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
private:
	// 设置为单例模式
	PageCache() {}
	PageCache(const PageCache& s1) = delete;
	SpanList _spanlist[NSPANLIST];
	static PageCache _sInst;
	//	使用Pageid 与 span 的对应
	/*std::unordered_map<PAGE_ID, Span*> _idSpanMap;*/

	// 使用基数树的优化
	TCMalloc_PageMap1<32 - SHIFT_SIZE> _idSpanMap;
	//	使用定长的内存池优化malloc
	ObjectPool<Span> _spanPool;
};