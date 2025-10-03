#include "ConcurrentAlloc.h"
//#include "ObjectPool.h"

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	//void* p4 = ConcurrentAlloc(7);
	//void* p5 = ConcurrentAlloc(8);
	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	//cout << p4 << endl;
	//cout << p5 << endl;
	//ConcurrentFree(p1, 6);
	//ConcurrentFree(p2, 8);
	//ConcurrentFree(p3, 1);
	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	//ConcurrentFree(p4, 7);
	//ConcurrentFree(p5, 8);
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; i++)
	{
		void* p1 = ConcurrentAlloc(6);
	}
	void* p2 = ConcurrentAlloc(6);
}

void BigAlloc()
{
	//ÕÒpage cacheÉêÇë
	void* p1 = ConcurrentAlloc(257 * 1024); //257KB
	ConcurrentFree(p1);

	//ÕÒ¶ÑÉêÇë
	void* p2 = ConcurrentAlloc(129 * 8 * 1024); //129Ò³
	ConcurrentFree(p2);
}

//int main()
//{
//	//TestObjectPool();
//	TestConcurrentAlloc1();
//	//TestConcurrentAlloc2();
//	//BigAlloc();
//	return 0;
//}