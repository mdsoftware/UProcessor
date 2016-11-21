/******************************************************************

  File Name 
      MEMMGR.H

  Description
      Memory manager. Optimized to be used with a large set of
      small or medium size blocks.

  Author
      Denis M
*******************************************************************/

#ifndef _MEMMGR_H
#define _MEMMGR_H

#include <WINDOWS.H>

#pragma pack(push, 1)
typedef struct _SHeapStatus
{
	DWORD dwUsed;
  LONGLONG lAllocated;
  LONGLONG lFreed;
  long lUsedBlockCount;
  long lFreeBlockCount;
  int iSegCount;
  DWORD dwSegMemory;
  char cHeapError[255];
  int iFreeCount;
  DWORD dwHeapUsed;
  int iCurFreeSeg;
} SHeapStatus, *PSHeapStatus;
#pragma pack(pop)

// Initialize heap
HRESULT WINAPI mmInit();

// Closes heap memory manager
HRESULT WINAPI mmClose();

// Allocate memory of requested size
// if result is NULL, no memory can be allocated
// or heap error occured. Use mmGetHeapStatus()
// and check cHeapError member of structure
// for detailed message
void * WINAPI mmAlloc(DWORD cbSize);

// Free pointer
// if result not S_OK (0), no memory can be allocated
// or heap error occured. Use mmGetHeapStatus()
// and check cHeapError member of structure
// for detailed message
HRESULT WINAPI mmFree(void *p);

// Compact heap
HRESULT WINAPI mmCompactHeap();

// Returns memory used in heap
DWORD WINAPI mmGetMemUsed();

// Checks if memory pointer belongs to a heap range
void * WINAPI mmCheckMemPtr(void *p);

// Return heap status in a record
HRESULT WINAPI mmGetHeapStatus(SHeapStatus *p);

//
// new and delete operators for object allocation
//
void *__cdecl operator new[] (size_t size);
void *__cdecl operator new(size_t size);

void __cdecl operator delete[] (void *p);
void __cdecl operator delete(void *p);

#endif // _MEMMGR_H