#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	Span* NewSpan(size_t k);

	Span* MapObjectToSpan(void* obj);

	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx;
private:
	SpanList _spanLists[NPAGES];
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;

	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;

	PageCache()
	{ }
	PageCache(const PageCache&) = delete;
	static PageCache _sInst;
};