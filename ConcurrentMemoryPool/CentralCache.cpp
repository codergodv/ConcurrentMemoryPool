#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;
ObjectPool<Span> SpanList::_spanPool;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = span->_freeList;
	size_t actualNum = 1;
	while (NextObj(end) && n - 1)
	{
		end = NextObj(end);
		actualNum++;
		n--;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;
	_spanLists[index]._mtx.unlock();
	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& spanList, size_t size)
{
	Span* it = spanList.Begin();
	while (it != spanList.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}
	spanList._mtx.unlock();
	PageCache::GetInstance()->_pageMtx.lock();

	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	spanList._mtx.lock();
	spanList.PushFront(span);
	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		NextObj(start) = span->_freeList;
		span->_freeList = start;

		span->_useCount--;
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_prev = nullptr;
			span->_next = nullptr;

			_spanLists[index]._mtx.unlock();
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();
			_spanLists[index]._mtx.lock();
		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();
}