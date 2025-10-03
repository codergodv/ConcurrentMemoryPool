#pragma once

#include <iostream>
#include <vector>
#include <ctime>
#include <cassert>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#include <atomic>
#include "ObjectPool.h"
//template<class T>
//class ObjectPool;


using std::cout;
using std::endl;

//thread cache最大能获取的内存大小
static const size_t MAX_BYTES = 256 * 1024;
//thread cache中哈希桶中自由链表的个数
static const size_t NFREELISTS = 208;
//page cache中哈希桶的个数
static const size_t NPAGES = 129;
////页大小转换，一页是2^13
//static const size_t PAGE_SHIFT = 13;

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
//Linux
#endif

#ifdef _WIN32
#include <Windows.h>
#else
	//
#endif

////使用系统调用按页（8kb）在堆上申请空间
//inline static void* SystemAlloc(size_t kpage)
//{
//#ifdef _WIN32
//	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//#else
//	//其他平台
//#endif
//	if (ptr == nullptr)
//		throw std::bad_alloc();
//	return ptr;
//}

//直接将内存还给堆
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	//linux下sbrk unmmap等
#endif
}

////传入一个地址，将地址的前四个字节以引用的方式返回
//static void*& NextObj(void* ptr)
//{
//	return (*(void**)ptr);
//}

//封装自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		_size++;
	}
	void* Pop()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(_freeList);
		_size--;
		return obj;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		assert(start);
		assert(end);

		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}

	void PopRange(void*& start, void*&end, size_t n)
	{
		assert(n <= _size);

		start = _freeList;
		end = start;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

//管理对齐和映射的关系
class SizeClass
{
public:
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8 * 1024);
		}
		else
		{
			//assert(false);
			return _RoundUp(bytes, 1 << PAGE_SHIFT);
		}
	}

	static inline size_t _Index(size_t bytes, size_t alignShift)
	{
		return ((bytes + ((1 << alignShift) - 1)) >> alignShift) - 1;
	}

	static inline size_t Index(size_t bytes)
	{
		static int arr[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes - 128, 4) + arr[0];
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + arr[0] + arr[1];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + arr[0] + arr[1] + arr[2];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 13) + arr[0] + arr[1] + arr[2] + arr[3];
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		int num = MAX_BYTES / size;

		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t nPage = num * size;
		nPage >>= PAGE_SHIFT;
		if (0 == nPage)
		{
			nPage = 1;
		}
		return nPage;
	}
};

//管理以页为单位的大块内存
struct Span
{
	PAGE_ID _pageId = 0;
	size_t _n = 0;

	Span* _next = nullptr;
	Span* _prev = nullptr;

	size_t _objSize = 0;
	size_t _useCount = 0;
	void* _freeList = nullptr;

	bool _isUse = false;
};

//带头双向循环链表
class SpanList
{
public:
	SpanList()
	{
		_head = _spanPool.New();
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	bool Empty()
	{
		return _head == _head->_next;
	}
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}
	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}
private:
	Span* _head;
	static ObjectPool<Span> _spanPool;
public:
	std::mutex _mtx;
};