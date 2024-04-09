#pragma once

#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using std::cout;
using std::endl;
using std::vector;

// 定义常量
static const size_t FREE_LIST_NUM = 208;
static const size_t MAX_BYTES = 256 * 1024;
static const size_t PAGE_NUM = 129;
static const size_t PAGE_SHIFT = 13;

typedef size_t PageID;

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

// 系统内存分配函数
inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
#else
    void* ptr = mmap(NULL, kpage << PAGE_SHIFT, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (ptr == nullptr) throw std::bad_alloc();
    return ptr;
}

// 系统内存释放函数
inline static void SystemFree(void* ptr) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, 0);
#endif
}

// 获取对象的下一个指针
static void*& objectNext(void* object) { return *(void**)object; }

// 自由链表类
class FreeList {
private:
    void* freeList = nullptr;  // 自由链表的起始指针
    size_t maxGetNum = 1;   // tc申请空间块的最大数量
    size_t size = 0;           // 链表大小

public:
    // 弹出一定数量的节点，并返回节点范围的起始和结束指针
    void popObjectRange(void*& start, void*& end, size_t objectNum) {
        assert(objectNum <= size);

        start = end = freeList;
        for (size_t i = 0; i < objectNum - 1; ++i) {
            end = objectNext(end);
        }

        freeList = objectNext(end);
        objectNext(end) = nullptr;
        size -= objectNum;
    }

    // 将一段节点范围推入链表
    void pushObjectRange(void* start, void* end, size_t objectNum) {
        objectNext(end) = freeList;
        freeList = start;
        size += objectNum;
    }

    // 推入一个节点到链表
    void pushObject(void* object) {
        assert(object);
        objectNext(object) = freeList;
        freeList = object;
        ++size;
    }

    // 弹出一个节点，并返回其指针
    void* popObject() {
        assert(freeList);
        void* object = freeList;
        freeList = objectNext(object);
        --size;
        return object;
    }

    // 向cc申请内存的最大数目
    size_t& maxFromCentral() { return maxGetNum; }
    // 返回链表大小
    size_t getSize() { return size; }

    bool empty() { return freeList == nullptr; }
};

// Span结构体，用于管理内存页
struct Span {
   public:
    PageID pageID = 0;     // 页号
    size_t pageSize = 0;   // 页面数
    size_t objectSize = 0; // 

    void* freeList = nullptr;
    size_t useCount = 0;

    Span* prev = nullptr;
    Span* next = nullptr;

    bool isUse = false;
};

// Span链表类，用于管理Span对象
class SpanList {
private:
    Span* head;  // 链表头指针

public:
    std::mutex spanListmtx;  // 互斥锁，用于多线程同步

public:

    // 构造函数，初始化头指针
    SpanList() : head(new Span) {
        head->next = head;
        head->prev = head;
    }

    // 弹出链表首部的Span对象
    Span* popFront() {
        Span* front = head->next;
        erase(front);
        return front;
    }

    // 从链表中删除指定的Span对象
    void erase(Span* span) {
        assert(span && span != head);

        span->prev->next = span->next;
        span->next->prev = span->prev;
    }

    // 在指定位置插入Span对象
    void insert(Span* pos, Span* span) {
        assert(pos && span);

        span->prev = pos->prev;
        span->next = pos;
        pos->prev->next = span;
        pos->prev = span;
    }

    // 判断链表是否为空
    bool empty() { return head == head->next; }

    // 将Span对象推入链表首部
    void pushFront(Span* span) { insert(begin(), span); }

    // 返回链表首部迭代器
    Span* begin() { return head->next; }

    // 返回链表尾部迭代器
    Span* end() { return head; }

};

class SizeClass {
private:
    // 对齐常数
    static const size_t ALIGNMENT_8 = 8;
    static const size_t ALIGNMENT_16 = 16;
    static const size_t ALIGNMENT_128 = 128;
    static const size_t ALIGNMENT_1024 = 1024;
    static const size_t ALIGNMENT_8K = 8 * 1024;
    static const size_t ALIGNMENT_64K = 64 * 1024;
    static const size_t ALIGNMENT_256K = 256 * 1024;

    // 索引组大小枚举
    enum {
        GROUP_SIZE_1 = 16,
        GROUP_SIZE_2 = 56
    };

    // 计算大小向上对齐到指定的对齐数
    static size_t alignSize(size_t size, size_t alignNum) {
        // size_t res = 0;
        // if (size % alignNum != 0) {
        //     res = (size / alignNum + 1) * alignNum;
        // } else {
        //     res = size;
        // }
        return ((size + alignNum - 1) & ~(alignNum - 1));
    }

    // 计算 size 对应的哈希表索引
    static inline size_t calculateIndex(size_t size, size_t alignShift) {
        // alignShift 表示对齐数的二进制位数，例如，8字节对齐时，alignShift 为3
        return ((size + (1 << alignShift) - 1) >> alignShift) - 1;
    }

public:
    // 将大小对齐到特定的尺寸
    static size_t alignToSize(size_t size) {
        if (size <= ALIGNMENT_128) {
            return alignSize(size, ALIGNMENT_8);
        } else if (size <= ALIGNMENT_1024) {
            return alignSize(size, ALIGNMENT_16);
        } else if (size <= ALIGNMENT_8K) {
            return alignSize(size, ALIGNMENT_128);
        } else if (size <= ALIGNMENT_64K) {
            return alignSize(size, ALIGNMENT_1024);
        } else if (size <= ALIGNMENT_256K) {
            return alignSize(size, ALIGNMENT_8K);
        } else {
            assert(false);
            return -1;
        }
    }


    // 计算 size 对应的自由链表桶索引
    static inline size_t getBucketIndex(size_t size) {
        assert(size <= MAX_BYTES);

        // 每个区间有多少个链
        static int groupArray[4] = {16, 56, 56, 56};
        if (size <= 128) { // [1,128]，对应 8 字节对齐，二进制位数为 3
            return calculateIndex(size, 3);
        } else if (size <= 1024) { // [128+1,1024]，对应 16 字节对齐，二进制位数为 4
            return calculateIndex(size - 128, 4) + groupArray[0];
        } else if (size <= 8 * 1024) { // [1024+1,8*1024]，对应 128 字节对齐，二进制位数为 7
            return calculateIndex(size - 1024, 7) + groupArray[1] + groupArray[0];
        } else if (size <= 64 * 1024) { // [8*1024+1,64*1024]，对应 1024 字节对齐，二进制位数为 10
            return calculateIndex(size - 8 * 1024, 10) + groupArray[2] + groupArray[1] + groupArray[0];
        } else if (size <= 256 * 1024) { // [64*1024+1,256*1024]，对应 8 * 1024 字节对齐，二进制位数为 13
            return calculateIndex(size - 64 * 1024, 13) + groupArray[3] + groupArray[2] + groupArray[1] + groupArray[0];
        } else {
            assert(false);
            return -1;
        }
    }

public:
    // tc单次向cc申请的上限
    static size_t maxAllocLimit(size_t size) {
        assert(size > 0);

        int num = MAX_BYTES / size;
        if (num > 512) num = 512;
        if (num < 2)   num = 2;

        return num;
    }

    // 计算内存页的移动数量
    static size_t numMovePage(size_t size) {
        size_t num = maxAllocLimit(size);
        size_t numPages = num * size;

        numPages >>= PAGE_SHIFT;

        if (numPages == 0) numPages = 1;

        return numPages;
    }
};
