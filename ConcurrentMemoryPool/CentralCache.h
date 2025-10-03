#pragma once

#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	Span* GetOneSpan(SpanList& spanList, size_t size);

	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanLists[NFREELISTS];
private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
	static CentralCache _sInst;
};