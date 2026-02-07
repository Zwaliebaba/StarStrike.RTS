//============================================================================
//
// File: interlocked.h
// Copyright (c) 2005-2006, Ensemble Studios
// Windows port: Removed Xbox 360 PowerPC memory barrier intrinsics
//
// Good reference:
// http://www.linuxjournal.com/article/8212
//============================================================================
#pragma once

namespace Sync
{
   //============================================================================
   // InterlockedIncrementExport
   //============================================================================
   inline LONG InterlockedIncrementExport(LONG volatile * lpAddend)
   {
      return InterlockedIncrement(lpAddend);
   }                                                             
   
   //============================================================================
   // InterlockedDecrementExport
   //============================================================================
   inline LONG InterlockedDecrementExport(LONG volatile * lpAddend)
   {
      return InterlockedDecrement(lpAddend);
   }      
   
   //============================================================================
   // InterlockedExchangeExport
   //============================================================================
   inline LONG InterlockedExchangeExport(LONG volatile * Target, LONG Value)
   {
      return InterlockedExchange(Target, Value);
   }        
   
   //============================================================================
   // CompareAndSwapAcquire
   //============================================================================
   inline BOOL CompareAndSwapAcquire(LONG volatile* pValue, LONG compValue, LONG newValue)
   {
      const LONG initialValue = InterlockedCompareExchange(pValue, newValue, compValue);
      return (initialValue == compValue);
   }                                           
   
   //============================================================================
   // CompareAndSwapRelease
   //============================================================================
   inline BOOL CompareAndSwapRelease(LONG volatile* pValue, LONG compValue, LONG newValue)
   {
      const LONG initialValue = InterlockedCompareExchange(pValue, newValue, compValue);
      return (initialValue == compValue);
   }  
   
   //============================================================================
   // CompareAndSwapMB
   // This is a conservative implementation - it doesn't know if you are using
   // release or acquire semantics so it's a full memory barrier.
   //============================================================================
   inline BOOL CompareAndSwapMB(LONG volatile* pValue, LONG compValue, LONG newValue)
   {
      const LONG initialValue = InterlockedCompareExchange(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
   //============================================================================
   // CompareAndSwap
   // Full memory barriers on Windows x86/x64 (implicit via Interlocked)
   //============================================================================
   inline BOOL CompareAndSwap(LONG volatile* pValue, LONG compValue, LONG newValue)
   {
      const LONG initialValue = InterlockedCompareExchange(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
   //============================================================================
   // CompareAndSwap64Acquire
   //============================================================================
   inline BOOL CompareAndSwap64Acquire(LONG64 volatile* pValue, LONG64 compValue, LONG64 newValue)
   {
      const LONG64 initialValue = InterlockedCompareExchange64(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
   //============================================================================
   // CompareAndSwap64Release
   //============================================================================
   inline BOOL CompareAndSwap64Release(LONG64 volatile* pValue, LONG64 compValue, LONG64 newValue)
   {
      const LONG64 initialValue = InterlockedCompareExchange64(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
   //============================================================================
   // CompareAndSwap64MB
   // This is a conservative implementation - it doesn't know if you are using
   // release or acquire semantics so it's a full memory barrier.
   //============================================================================
   inline BOOL CompareAndSwap64MB(LONG64 volatile* pValue, LONG64 compValue, LONG64 newValue)
   {
      const LONG64 initialValue = InterlockedCompareExchange64(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
   //============================================================================
   // CompareAndSwap64
   //============================================================================
   inline BOOL CompareAndSwap64(LONG64 volatile* pValue, LONG64 compValue, LONG64 newValue)
   {
      const LONG64 initialValue = InterlockedCompareExchange64(pValue, newValue, compValue);
      return (initialValue == compValue);
   }
   
}
