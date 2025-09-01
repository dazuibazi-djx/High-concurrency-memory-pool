#pragma once
#include "common.h"
class CenterCache
{
public:
	static CenterCache* GetInstance()
	{
		return &_sInst;
	}
	// ��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);

	// �����Ļ����ȡһ�������Ķ����thread cache�� ��������Ͳ����ķ�ʽ
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// �ͷ���Դ��thread �� center ��spans��
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanLists[NFREELIST];	// �涨�� ���Ͱ��Ȼ������� Span ��һ������Ľṹ�壬������common.h ��
//	ʹ�õ���ģʽ������ģʽ�����ڴ��Լ�����һ��������û�п�ʼ��ʱ��ʹ����ö���
private:
	CenterCache()
	{}

	CenterCache(const CenterCache&) = delete;

	static CenterCache _sInst;
};