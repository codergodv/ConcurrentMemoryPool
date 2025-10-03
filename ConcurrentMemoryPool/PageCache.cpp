#include "PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		//Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);
		return span;
	}

	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();
		for (PAGE_ID i = 0; i < kSpan->_n; i++)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}
		return kSpan;
	}

	for (int i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			//Span* kSpan = new Span;
			Span* kSpan = _spanPool.New();
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;
			_spanLists[nSpan->_n].PushFront(nSpan);

			//_idSpanMap[nSpan->_pageId] = nSpan;
			//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;

			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			for (PAGE_ID i = 0; i < kSpan->_n; i++)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}

			return kSpan;
		}
	}

	void* ptr = SystemAlloc(NPAGES - 1);
	//Span* maxSpan = new Span;
	Span* maxSpan = _spanPool.New();
	maxSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	maxSpan->_n = NPAGES - 1;

	_spanLists[NPAGES - 1].PushFront(maxSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
	//std::unique_lock<std::mutex> lock(_pageMtx);
	//auto ret = _idSpanMap.find(id);
	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	assert(false);
	//	return nullptr;
	//}
	Span* ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}


	while (1)
	{
		PAGE_ID prev = span->_pageId - 1;
		//auto ret = _idSpanMap.find(prev);
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		Span* ret = (Span*)_idSpanMap.get(prev);
		if (ret == nullptr)
		{
			break;
		}
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true)
		{
			break;
		}
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;
		_spanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}
	while (1)
	{
		PAGE_ID next = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(next);
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		Span* ret = (Span*)_idSpanMap.get(next);
		if (ret == nullptr)
		{
			break;
		}
		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}
		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		span->_n += nextSpan->_n;
		_spanLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}
	_spanLists[span->_n].PushFront(span);

	//_idSpanMap[span->_pageId] = span;
	//_idSpanMap[span->_pageId + span->_n - 1] = span;

	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);

	span->_isUse = false;
}