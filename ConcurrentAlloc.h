#pragma once
#include "ThreadCache.h"

void* concurrentAlloc(size_t size) {
    if (pTLSThreadCache == nullptr) {
        pTLSThreadCache = new ThreadCache();
    }

    return pTLSThreadCache->allocate(size);
}

void concurrentFree(void* object, size_t size) {
    assert(object);

    pTLSThreadCache->deallocate(object, size);
}