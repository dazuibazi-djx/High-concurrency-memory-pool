#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include "common.h"
#include <Windows.h>
using std::cout;
using std::endl;
//为了进一步提高申请内存的效率使用函数，直接去堆上按⻚申请空间
//inline static void* SystemAlloc(size_t kpage)
//{
//#ifdef _WIN32
//	void* ptr = VirtualAlloc(0, kpage * (1 << 12), MEM_COMMIT | MEM_RESERVE,
//		PAGE_READWRITE);
//#else
//	// linux下brk mmap等
//#endif
//}
template<class T>
class ObjectPool
{
public:
	//1. 实现new
	T* New()
	{
		T * obj = nullptr; // new返回的一个对象
		if (_freelist)
		{
			//如果 freelist里面有的话直接返回 freeList里面的然后， freeList进行向后移动
			auto next = *(void**)_freelist; // 使用 void**，表示指向 指针的指针，方便在 32 位或者是 64 确定下一个地址的大小
			obj = (T*)_freelist;
			_freelist = next;
		}
		else
		{
			// 剩下的内存不够
			if (leftSize < sizeof(T))
			{
				//空间不够就要进行开辟，假设为 128 kb的字节
				leftSize = 128 * 1024;
				_memory = (char*)malloc(128 * 1024);
				_memory = (char*)SystemAlloc(leftSize >> 13);
			}
			size_t needSize = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*); // 处理情况为 int 类型但是是在 64 位下空间不够的情况
			obj = (T*)_memory;
			_memory += needSize;
			leftSize -= needSize;	
		}
		//处理自定义类型
		new(obj)T;
		return obj;
	}

	void Delete(T* obj)
	{
		//调用析构函数
		obj->~T();
		//不需要释放直接放到 _freeList 中使用头插的方式
		*(void**)obj = _freelist;
		_freelist = obj;
	}
private:
	//定长内存池需要使用三个变量
	char* _memory = nullptr;   // 一次申请的大块内存
	size_t leftSize = 0;	   //剩下的内存的daxiao
	void* _freelist = nullptr; // 释放内存的存放位置
};
//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};
//void TestObjectPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 3;
//	// 每轮申请释放多少次
//	const size_t N = 100000;
//	size_t begin1 = clock();
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//		v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//		delete v1[i];
//		}
//		v1.clear();
//	}
//	size_t end1 = clock();
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < 100000; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//	cout << "new cost time:" << end1 - begin1 << endl;
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}

