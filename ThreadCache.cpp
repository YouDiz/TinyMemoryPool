#include "ThreadCache.h"
#include "CentralCache.h"

// 线程向tc申请size大小的空间
void* ThreadCache::allocate(size_t size) {
    assert(size <= MAX_BYTES);  // tc中单次只能申请不超过256KB的空间

    size_t alignSize = SizeClass::alignToSize(size);  // size对齐后的字节数
    size_t index = SizeClass::getBucketIndex(size);  // size对应在哈希表中的哪个桶

    if (!freeLists[index].empty()) {  
        return freeLists[index].popObject();
    } else { 
        return fetchFromCentralCache(index, alignSize);  
    }
}

// 回收线程中大小为size的obj空间
void ThreadCache::deallocate(void* object, size_t size) {
    assert(object);             
    assert(size <= MAX_BYTES);  
    // 回收的内存就不用对齐了
    size_t index = SizeClass::getBucketIndex(size); // 找到size对应的自由链表
    freeLists[index].pushObject(object);            // 用对应自由链表回收空间

    // 当前桶中的块数大于等于单批次申请块数的时候归还空间
    if (freeLists[index].getSize() >= freeLists[index].maxFromCentral()) {
        listTooLong(freeLists[index], size);
    }
}

void* ThreadCache::fetchFromCentralCache(size_t index, size_t alignSize) {
    size_t batchNum = std::min(freeLists[index].maxFromCentral(), 
                                SizeClass::maxAllocLimit(alignSize));
    if (batchNum == freeLists[index].maxFromCentral()) {
        // 慢反馈调节
        freeLists[index].maxFromCentral() ++;
    }

    // 返回的空间
    void* start = nullptr;
    void* end = nullptr;
    
    // 实际获取的空间可能不会等于batchNum，单个span可能不够
    size_t actualGetNum = CentralCache::getInstance()
    ->fetchRangeObject(start, end, batchNum, alignSize);

    // 如果只得到一个内存，直接返回给线程
    if (actualGetNum == 1) {
        assert(start == end);
        return start;
    } else {
        freeLists[index].pushObjectRange(objectNext(start), end, actualGetNum - 1);
        return start;
    }
}