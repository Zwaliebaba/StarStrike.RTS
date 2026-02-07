//=============================================================================
//
//  Memory.cpp
//
//  Copyright (c) 1999-2002, Ensemble Studios
//  Windows port: Xbox-specific memory functions removed
//
//=============================================================================
#include "xcore.h"
#include "threading\eventDispatcher.h"

#define TRACEMEM_ENABLED 0

#ifndef BUILD_FINAL
//==============================================================================
// traceMem
//==============================================================================
void traceMem(const char* file, long line)
{
   file;
   line;
#if TRACEMEM_ENABLED

   static DWORD traceTime=0;
   static DWORD startTime=0;
   DWORD curTime=GetTickCount();
   if(startTime==0)
      startTime=curTime;
   DWORD totalElapsedTime=curTime-startTime;
   DWORD elapsedTime=0;
   if(traceTime!=0)
      elapsedTime=curTime-traceTime;
   traceTime=curTime;

   MEMORYSTATUS status;
   ZeroMemory(&status, sizeof(status));
   GlobalMemoryStatus(&status);

   const uint MB = 1024U*1024U;
   
   BFixedString<256> buf;

   buf.format(
      "%s(%d) *TRACEMEM* thread=%u curTime=%u elapsed=%u totalElapsed=%u curAlloc=%u, physAlloc=%uMB, physFree=%uMB, virtAlloc=%uMB, virtFree=%uMB\n", 
      file, line, 
      gEventDispatcher.getThreadIndex(),
      curTime,
      elapsedTime/1000, totalElapsedTime/1000,
      0, 
      (status.dwTotalPhys - status.dwAvailPhys)/MB, status.dwAvailPhys/MB, 
      (status.dwTotalVirtual-status.dwAvailVirtual)/MB, status.dwAvailVirtual/MB );
      
   OutputDebugStringA(buf);      
#endif   
}
#endif

// Windows port: verifyAllHeaps is a no-op
void verifyAllHeaps(void)
{
}