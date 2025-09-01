#include "ConcurrentAlloc.h"
void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(7);
	}
}


void TLSTest()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}
void TestConcurrentAlloc1()
{	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(2);
	void* p3 = ConcurrentAlloc(3);
	void* p4 = ConcurrentAlloc(4);
	void* p5 = ConcurrentAlloc(6);
	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
}
void TestConcurrentAlloc2()
{
	for (int i = 0; i < 1024; i++)
	{
		void* p2 = ConcurrentAlloc(6);
		cout << p2 << endl;
	}
	void* p1 = ConcurrentAlloc(6);
	cout << p1;
}
//int main()
//{
//	//TestObjectPool();
//	//TLSTest();
//	TestConcurrentAlloc1();
//	//TestConcurrentAlloc2();
//	return 0;
//}