#include "ThreadCache.h"
#include "CenterCache.h"
void* ThreadCache::Allocate(size_t size)
{
	// Ҫ�ռ�����Ҫ�� size ��Ҫ�Ŀռ��С���ж��룬�ҶԶ����С��Ȼ����ȥͰ�����ң�Ȼ����� pop ����
	// ���ж����Ŀ��Ҳ��������޶ȵļ�������Ƭ������
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	//���ٿռ俴����Ӧ�� freelist ������û�������ֱ�� pop�� û�о�Ҫ����һ��ȥҪ
	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].pop();
	}
	else
	{
		// ����Ͱ���±꣬�Լ�����õĴ�С��
		return FetchFromCenterCache(index, alignSize);
	}
}
void* ThreadCache::FetchFromCenterCache(size_t index, size_t size)
{
	//	�� CenterCache �л��һЩ���ֺõ��ڴ��
	//	ʹ�����������㷨
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));

	if (batchNum == _freeLists[index].MaxSize()) _freeLists[index].MaxSize()++;
	void* start = nullptr;
	void* end = nullptr;
	// �� centercahche �л����Ӧ��С��С���ڴ棬�� span �к�
	size_t actualNum = CenterCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum >= 1);
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		// �� start ����ѭ����������Ȼ�󷵻� start 
		// ����һ������֮��Ҫ�Ѻ���� n - 1 ��������
		_freeLists[index].pushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
} 

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// ����ɾ�����ǷŻص�������������
	size_t index = SizeClass::Index(size);

	_freeLists[index].push(ptr);
	//�����һ�����������ĸ�������һ�������������������ô������Ҫ���� CenterCache
	if (_freeLists[index].size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
}
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	// ������� list �Ŀ�ʼ�����λ��
	// 1.�ȴ� list ����������Ȼ��������Ļ���ĺ��� ReleaseListToSpans
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	// ʹ�ÿ�ʼλ�ã��Լ�����ֱ�ӵ������ǵĺ���
	CenterCache::GetInstance()->ReleaseListToSpans(start, size);

}