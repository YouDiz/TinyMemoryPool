#include "Common.h"

template<class T>
class StaticMemoryPool {
private:
    char *memory = nullptr;
    size_t remainingBytes = 0; // 剩余的字节数
    void *freeMemoryList = nullptr;

public:
    std::mutex poolMutex;

public:
    // 申请一个T类型大小的空间 
    T* newMemory() {
        T* object = nullptr;

        if (freeMemoryList) {
            void* next = *(void**) freeMemoryList; // next指针指向下一个内存块
            object = (T*)freeMemoryList;
            freeMemoryList = next;
        } else {
            if (remainingBytes < sizeof(T)) {
                remainingBytes = 128 * 1024; // 开辟128KB内存
                memory = (char*)SystemAlloc(remainingBytes >> 13); // 右移13位，表示除以8KB，就是16页内存
                if (memory == nullptr) throw std::bad_alloc(); // 分配失败
            }

            object = (T*)memory; // 切出一个T大小的内存
            // 分配至少一个指针大小
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T); 
            memory += objSize;
            remainingBytes -= objSize;
        }

        new (object) T; // 定位new调用构造函数
        return object;
    }

    // 回收还回来的小空间
    void deleteMemory(T* object) {
        object->~T();
        *(void**)object = freeMemoryList;  // 直接从obj前几个字指针指向自由链表
        freeMemoryList = object; 
    }
};


