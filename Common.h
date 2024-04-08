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
static void*& ObjNext(void* obj) { return *(void**)obj; }
