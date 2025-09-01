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
// 根据规定 64 位下也会有 32，改变即可解决问题
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif
	

// 使用系统函数在向堆上去申请内存
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}
// 使用系统返回，将内存返回到堆上面
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

using std::cout;
using std::endl;
//确定 Thread 当中的最大的字节数，桶的大小
static const size_t MAX_BYTES = 1024 * 256; // 单次申请内存最大的空间大小为 256 kb
static const size_t NFREELIST = 208;		// CenterCache 整个桶的大小为 208个
static const size_t NSPANLIST = 129;		//	PageCahe 整个桶的大小为 129；
static const size_t SHIFT_SIZE = 13;		// 表示 8 * 1024, 也就是 2 的 13 次方
//创建Thread Cache 桶的结构,也就是管理小块内存的一个链表
static void*& NextObj(void* ptr)
{
	return *(void**)ptr; // 保证在 32，64 位下同时使用
}
class FreeList
{
public:
	void push(void* ptr)
	{
		assert(ptr);
		//直接将 ptr 头插链表即可,让这个 ptr 的前几个字节表示下一个链表结点的地址
		NextObj(ptr) = _freeList; // 添加引用实现对于对象的修改
		_freeList = ptr;
		_size++;
	}
	void* pop()
	{
		//删除就是将结点进行返回，头结点指向下一个结点
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
		// 将尾部的下一个指向 _freeList, 然后是 start 赋值为_freeList
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}
	void PopRange(void*& start, void*& end, size_t n)
	{
		// 从一个链上记录下来要删除的区间
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
	//对齐规则为：
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)
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
	// 判断是哪一个桶， 就让它除以对齐数，计算是第几个桶即可
	static size_t _Index(size_t size, size_t n)
	{
		return ((size + (1 << n) - 1) >> n) - 1;
	}
	//确定是哪个桶
	static inline size_t Index(size_t size)
	{
		assert(size <= 256 * 1024);
		// 设置前面的桶的大小
		static int arr[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{
			return _Index(size, 3); // 除以 8 
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
			// 查找不到强制退出
			assert(false);
		}
		return -1;	//最后没有就是要退出 -1
	}
	static size_t NumMoveSize(size_t size)
	{
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}
	static size_t NumMovePage(size_t size)
	{
		// 因为要给 span 的大小规定为 8 kb
		size_t nsize = NumMoveSize(size); // 得到我要的最多的个数
		size_t npage = nsize * size;
		npage = npage >> SHIFT_SIZE;	  // 一个 page 的页数为 8 * 1024 
		if (npage == 0) return 1;
		else return npage;				  // 判断出在那个页里面去找
	}

};
struct Span
{
	size_t  _n = 0;      // 页的数量
	PAGE_ID _pageId = 0; // 大块内存起始页的页号, 在不同的系统下可能会达到 long long 的级别
	void* freelist = nullptr;
	// 定义为双向循环的结构，方便进行删除，或者是使用
	Span* next = nullptr;
	Span* prev = nullptr;
	//	大块内存里面的小块内存的使用个数
	size_t _useCount = 0;
	size_t _objSize = 0; //  切好的小对象的大小
	bool _isUsed = false; // 判断这个 span 是空闲的的还是说一直正在被 centercache 以及 threadcache 进行使用
};
// 有 span 组成的 spanlist
class SpanList
{
public:
	SpanList()
	{
		// 结构为一个带头的双向循环链表
		_head = new Span;
		_head->next = _head->prev = _head;
	}
	void Insert(Span* span, Span* newspan)
	{
		// 将newspan 放到 span 对应的这个_head结构里面
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
		// 不可删除头节点
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
		Insert(Begin(), span); // 不是插到头的前面是，插入到一开始位置的前面
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
	std::mutex _mtx; // 桶锁
};