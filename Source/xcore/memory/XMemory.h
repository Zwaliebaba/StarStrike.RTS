//============================================================================
//
//  Memory.h
//
//  Copyright (c) 1999-2001, Ensemble Studios
//
//============================================================================
#pragma once

#ifndef BUILD_FINAL
   void traceMem(const char* file, long line);
   #define TRACEMEM  traceMem(__FILE__, __LINE__);
#else
   #define TRACEMEM
#endif

#include "memoryHeap.h"

void verifyAllHeaps(void);

// Windows port: Use C Runtime heap for all allocations
const eHeapType cPrimaryHeapType = cCRunTimeHeap;
const eHeapType cRenderHeapType = cCRunTimeHeap;
const eHeapType cDefaultHeapType = cCRunTimeHeap;
const eHeapType cSmallHeapType = cCRunTimeHeap;
const eHeapType cDefaultPhysicalHeapType = cCRunTimeHeap;

// Windows does not use custom aligned allocators
const bool cMemoryAllocatorsSupportAlignment = false;

// Thread-safe primary heap
extern BMemoryHeap gPrimaryHeap;

// Single threaded sim heap
extern BMemoryHeap gSimHeap;

// Single threaded network heap
extern BMemoryHeap gNetworkHeap;

// Thread-safe render heap
extern BMemoryHeap gRenderHeap;

// Thread-safe file block heap (used during archive loading to minimize memory fragmentation)
extern BMemoryHeap gFileBlockHeap;

// C Run Time library heap
extern BMemoryHeap gCRunTimeHeap;

// Physical Heaps
extern BMemoryHeap gPhysCachedHeap;
extern BMemoryHeap gPhysWriteCombinedHeap;

extern BMemoryHeap gSyncHeap;

// Xbox-specific XPhysicalAlloc wrappers removed for Windows port
// Standard malloc/free/realloc are used instead