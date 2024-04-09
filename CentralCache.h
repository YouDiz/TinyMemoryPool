#pragma once
#include "Common.h"

class CentralCache {
private:
    SpanList spanLists[FREE_LIST_NUM]; // 哈希桶中挂的是Span

private:
    CentralCache() {} // 构造函数私有化，禁止外部创建实例
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator =(const CentralCache& copy) = delete;

public:
    // cc从自己的_spanLists中为tc提供tc所需要的块空间
    size_t fetchRangeObject(void*& start, void*& end, size_t batchNum, size_t size);

	// 获取一个管理空间不为空的span
	Span* getOneSpan(SpanList& list, size_t size);

	// 将tc还回来的多块空间放到span中
	void releaseListToSpans(void* start, size_t size);

    // 获取单例对象的静态成员函数
    static CentralCache* getInstance() {
        // 单列对象
        static CentralCache instance;
        return &instance;
    }
};
