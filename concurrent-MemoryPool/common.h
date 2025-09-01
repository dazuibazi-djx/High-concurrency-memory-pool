#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include <thread>
#include <assert.h>
#include <mutex>
#include <algorithm>
#include <windows.h>
#include <unordered_map>
#include <atomic>
// ���ݹ涨 64 λ��Ҳ���� 32���ı伴�ɽ������
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif
	

// ʹ��ϵͳ�����������ȥ�����ڴ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}
// ʹ��ϵͳ���أ����ڴ淵�ص�������
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}

using std::cout;
using std::endl;
//ȷ�� Thread ���е������ֽ�����Ͱ�Ĵ�С
static const size_t MAX_BYTES = 1024 * 256; // ���������ڴ����Ŀռ��СΪ 256 kb
static const size_t NFREELIST = 208;		// CenterCache ����Ͱ�Ĵ�СΪ 208��
static const size_t NSPANLIST = 129;		//	PageCahe ����Ͱ�Ĵ�СΪ 129��
static const size_t SHIFT_SIZE = 13;		// ��ʾ 8 * 1024, Ҳ���� 2 �� 13 �η�
//����Thread Cache Ͱ�Ľṹ,Ҳ���ǹ���С���ڴ��һ������
static void*& NextObj(void* ptr)
{
	return *(void**)ptr; // ��֤�� 32��64 λ��ͬʱʹ��
}
class FreeList
{
public:
	void push(void* ptr)
	{
		assert(ptr);
		//ֱ�ӽ� ptr ͷ��������,����� ptr ��ǰ�����ֽڱ�ʾ��һ��������ĵ�ַ
		NextObj(ptr) = _freeList; // �������ʵ�ֶ��ڶ�����޸�
		_freeList = ptr;
		_size++;
	}
	void* pop()
	{
		//ɾ�����ǽ������з��أ�ͷ���ָ����һ�����
		void* cur = _freeList;
		_freeList = NextObj(_freeList);
		_size--;
		return cur;
	}
	bool Empty()
	{
		return _freeList == nullptr;
	}
	size_t& MaxSize()
	{
		return maxsize;
	}
	void pushRange(void* start, void* end, size_t n)
	{
		// ��β������һ��ָ�� _freeList, Ȼ���� start ��ֵΪ_freeList
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}
	void PopRange(void*& start, void*& end, size_t n)
	{
		// ��һ�����ϼ�¼����Ҫɾ��������
		assert(n <= _size);
		start = end = _freeList;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}
	size_t size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t maxsize = 1;
	size_t _size = 0;
};
class SizeClass
{
public:
	//�������Ϊ��
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	    freelist[0,16)
	// [128+1,1024]				16byte����	    freelist[16,72)
	// [1024+1,8*1024]			128byte����	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte����   freelist[184,208)
	static inline size_t _RoundUp(size_t size, size_t n)
	{
		
		size_t alignSize;
		if (size % n != 0)
		{
			alignSize = (size / n + 1)*n;
		}
		else
		{
			alignSize = size;
		}

		return alignSize;
	}
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if(size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else 
		{
			return _RoundUp(size, 1 << SHIFT_SIZE);
		}
	}
	// �ж�����һ��Ͱ�� ���������Զ������������ǵڼ���Ͱ����
	static size_t _Index(size_t size, size_t n)
	{
		return ((size + (1 << n) - 1) >> n) - 1;
	}
	//ȷ�����ĸ�Ͱ
	static inline size_t Index(size_t size)
	{
		assert(size <= 256 * 1024);
		// ����ǰ���Ͱ�Ĵ�С
		static int arr[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{
			return _Index(size, 3); // ���� 8 
		}
		else if (size <= 1024)
		{
			return _Index(size - 128, 4) + arr[0];
		}
		else if (size <= 8 * 1024) {
			return _Index(size - 1024, 7) + arr[1] + arr[0];
		}
		else if (size <= 64 * 1024) {
			return _Index(size - 8 * 1024, 10) + arr[2] + arr[1] + arr[0];
		}
		else if (size <= 256 * 1024) {
			return _Index(size - 64 * 1024, 13) + arr[3] + arr[2] + arr[1] + arr[0];
		}
		else 
		{
			// ���Ҳ���ǿ���˳�
			assert(false);
		}
		return -1;	//���û�о���Ҫ�˳� -1
	}
	static size_t NumMoveSize(size_t size)
	{
		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
		// С����һ���������޸�
		// С����һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}
	static size_t NumMovePage(size_t size)
	{
		// ��ΪҪ�� span �Ĵ�С�涨Ϊ 8 kb
		size_t nsize = NumMoveSize(size); // �õ���Ҫ�����ĸ���
		size_t npage = nsize * size;
		npage = npage >> SHIFT_SIZE;	  // һ�� page ��ҳ��Ϊ 8 * 1024 
		if (npage == 0) return 1;
		else return npage;				  // �жϳ����Ǹ�ҳ����ȥ��
	}

};
struct Span
{
	size_t  _n = 0;      // ҳ������
	PAGE_ID _pageId = 0; // ����ڴ���ʼҳ��ҳ��, �ڲ�ͬ��ϵͳ�¿��ܻ�ﵽ long long �ļ���
	void* freelist = nullptr;
	// ����Ϊ˫��ѭ���Ľṹ���������ɾ����������ʹ��
	Span* next = nullptr;
	Span* prev = nullptr;
	//	����ڴ������С���ڴ��ʹ�ø���
	size_t _useCount = 0;
	size_t _objSize = 0; //  �кõ�С����Ĵ�С
	bool _isUsed = false; // �ж���� span �ǿ��еĵĻ���˵һֱ���ڱ� centercache �Լ� threadcache ����ʹ��
};
// �� span ��ɵ� spanlist
class SpanList
{
public:
	SpanList()
	{
		// �ṹΪһ����ͷ��˫��ѭ������
		_head = new Span;
		_head->next = _head->prev = _head;
	}
	void Insert(Span* span, Span* newspan)
	{
		// ��newspan �ŵ� span ��Ӧ�����_head�ṹ����
		assert(span && newspan);

		Span* next = span->next;
		Span* prev = span->prev;
		prev->next = newspan;
		newspan->prev = prev;
		newspan->next = span;
		span->prev = newspan;
	}
	void Earse(Span* span)
	{
		assert(span);
		// ����ɾ��ͷ�ڵ�
		assert(span != _head);
		Span* next = span->next;
		Span* prev = span->prev;
		prev->next = next;
		next->prev = prev;
	}
	Span* Begin()
	{
		return _head->next;
	}
	Span* End()
	{
		return _head;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span); // ���ǲ嵽ͷ��ǰ���ǣ����뵽һ��ʼλ�õ�ǰ��
	}
	Span* PopFront()
	{
		Span* front = _head->next;
		Earse(front);
		return front;
	}
	bool Empty()
	{
		return  _head->next == _head;
	}
private:
	Span*  _head;
public:
	std::mutex _mtx; // Ͱ��
};