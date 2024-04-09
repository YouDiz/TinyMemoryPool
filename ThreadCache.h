#include "Common.h"

class ThreadCache {
private:
    FreeList freeLists[FREE_LIST_NUM];

public:
    // 线程申请size大小的空间
    void* allocate(size_t size);

    // 回收线程中大小为size的obj空间
    void deallocate(void* object, size_t size);

    // ThreadCache中空间不够时，向CentralCache申请空间的接口
	void* fetchFromCentralCache(size_t index, size_t alignSize);

	// tc向cc归还空间List桶中的空间
	void listTooLong(FreeList& list, size_t size);

};

// TLS的全局对象的指针，这样每个线程都能有一个独立的全局对象
// static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr; // ==> _declspec(thread)是Windows特有的，不是所有编译器都支持
//注意要给成static的，不然当多个.cpp文件包含该文件的时候会发生链接错误

static thread_local ThreadCache* pTLSThreadCache = nullptr; // thread_local是C++11提供的，能跨平台