#include "CentralCache.h"

size_t CentralCache::fetchRangeObject(void*& start, void*& end, size_t batchNum,
                                      size_t size) {
    size_t index = SizeClass::getBucketIndex(size);
    
    // 在CentralCache中获取内存需要加锁
    spanLists[index].spanListmtx.lock();
    Span* span = getOneSpan(spanLists[index], size);
    assert(span);
    assert(span->freeList);

    // 起初都指向_freeList，让end不断往后走
	start = end = span->freeList;
	size_t actualNum = 1; 

	// 在end的next不为空的前提下，让end走batchNum - 1步
	size_t i = 0;
	while (i < batchNum - 1 && objectNext(end) != nullptr) {
		end = objectNext(end);
		++actualNum; // 记录end走过了多少步
		++i;
	}

	// 将[start, end]返回给ThreadCache后，调整Span的_freeList
	span->freeList = objectNext(end);
	span->useCount += actualNum; // 给tc分了多少就给useCount加多少
	// 返回一段空间，不要和原先Span的_freeList中的块相连
	objectNext(end) = nullptr; 
    spanLists[index].spanListmtx.unlock();

    return actualNum;
}
