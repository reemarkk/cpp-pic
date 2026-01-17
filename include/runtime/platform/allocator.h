#pragma once

#include "primitives.h"

extern "C" PVOID memset(PVOID dest, INT32 ch, USIZE count);
extern "C" PVOID memcpy(PVOID dest, const VOID *src, USIZE count);
extern "C" INT32 memcmp(const VOID *ptr1, const VOID *ptr2, USIZE num);

class Allocator
{
private:
    /* data */
public:
    // Platform-specific allocation (implemented in platform-specific .cc files)
    static PVOID AllocateMemory(USIZE size);
    static VOID ReleaseMemory(PVOID ptr, USIZE size);

    // Inline memory operations (zero overhead wrappers)
    FORCE_INLINE static PVOID CopyMemory(PVOID dest, PCVOID src, USIZE count)
    {
        return memcpy(dest, src, count);
    }

    FORCE_INLINE static INT32 CompareMemory(PCVOID ptr1, PCVOID ptr2, USIZE num)
    {
        return memcmp(ptr1, ptr2, num);
    }

    FORCE_INLINE static PVOID SetMemory(PVOID dest, INT32 ch, USIZE count)
    {
        return memset(dest, ch, count);
    }
};
