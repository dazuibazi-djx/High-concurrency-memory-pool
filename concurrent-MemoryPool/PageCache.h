#pragma once
#include "common.h"
#include "PageMap.h"
#include "objectpool.h"
// �� page ����ṹ����һ�� 128 ҳ��spanͰ���ɵ�һ�� spanlist
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	// ��Page���� span �� Centercache, ��Ҫֱ���Ǵ��ĸ�ҳ����Ѱ�ҵ�span
	Span* NewSpan(size_t k);
	std::mutex _pageMtx;
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);
private:
	// ����Ϊ����ģʽ
	PageCache() {}
	PageCache(const PageCache& s1) = delete;
	SpanList _spanlist[NSPANLIST];
	static PageCache _sInst;
	//	ʹ��Pageid �� span �Ķ�Ӧ
	/*std::unordered_map<PAGE_ID, Span*> _idSpanMap;*/

	// ʹ�û��������Ż�
	TCMalloc_PageMap1<32 - SHIFT_SIZE> _idSpanMap;
	//	ʹ�ö������ڴ���Ż�malloc
	ObjectPool<Span> _spanPool;
};