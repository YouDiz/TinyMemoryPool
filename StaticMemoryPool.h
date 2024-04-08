#include "Common.h"

template<class T>
class StaticMemoryPool {
private:
    char *memory = nullptr;
    size_t freeBytes = 0; // 剩余的字节数
    void *freeList = nullptr;

public:
    std::mutex poolMtx;

public:
    // 申请一个T类型大小的空间 
    T* newMemory() {
        T* obj = nullptr;

        if (freeList) {
            void* next = *(void**) freeList; // next指针指向下一个内存块
            obj = (T*)freeList;
            freeList = next;
        } else {
            if (freeBytes < sizeof(T)) {
                freeBytes = 128 * 1024; // 开辟128KB内存
                memory = (char*)SystemAlloc(freeBytes >> 13); // 右移13位，表示除以8KB，就是16页内存
                if (memory == nullptr) throw std::bad_alloc(); // 分配失败
            }

            obj = (T*)memory; // 切出一个T大小的内存
            // 分配至少一个指针大小
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T); 
            memory += objSize;
            freeBytes -= objSize;
        }

        new (obj) T; // 定位new调用构造函数
        return obj;
    }

    // 回收还回来的小空间
    void deleteMemory(T* obj) {
        obj->~T();
        *(void**)obj = freeList;  // 直接从obj前几个字指针指向自由链表
        freeList = obj; 
    }
};


