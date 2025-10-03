#pragma once

//页大小转换，一页是2^13
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN32
#include <Windows.h>
#else
//
#endif

//传入一个地址，将地址的前四个字节以引用的方式返回
static void*& NextObj(void* ptr)
{
	return (*(void**)ptr);
}

//使用系统调用按页（8kb）在堆上申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//其他平台
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

//定长内存池
template<class T>
class ObjectPool
{
public:
	//申请对象
	T* New()
	{
		T* obj = nullptr;

		if (_freeList != nullptr)
		{
			obj = (T*)_freeList;
			_freeList = NextObj(_freeList);
		}
		else
		{
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			if (_remainBytes < objSize)
			{
				_remainBytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_remainBytes >> 13);
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			_memory += objSize;
			_remainBytes -= objSize;
		}
		new(obj)T;
		return obj;
	}

	//释放对象
	void Delete(T* obj)
	{
		obj->~T();
		
		NextObj(obj) = _freeList;
		_freeList = obj;
	}
private:
	char* _memory = nullptr;
	size_t _remainBytes = 0;
	void* _freeList = nullptr;
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
//	{
//	}
//};

//void TestObjectPool()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 3;
//	// 每轮申请释放多少次
//	const size_t N = 1000000;
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	//malloc和free
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode);
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i];
//		}
//		v1.clear();
//	}
//	size_t end1 = clock();
//
//	//定长内存池
//	ObjectPool<TreeNode> TNPool;
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New());
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	cout << "new cost time:" << end1 - begin1 << endl;
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}
