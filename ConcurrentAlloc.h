#pragma once
#include "ThreadCache.h"

void* concurrentAlloc(size_t size) {
    cout << std::this_thread::get_id() << " " << pTLSThreadCache << endl;
    if (pTLSThreadCache == nullptr) {
        pTLSThreadCache = new ThreadCache();
    }

    return pTLSThreadCache->allocate(size);
}

void concurrentFree(void* object, size_t size) {
    assert(object);

    pTLSThreadCache->deallocate(object, size);
}