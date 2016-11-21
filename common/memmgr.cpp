/******************************************************************

  File Name 
      MEMMGR.CPP

  Description
      Function declaration for memory manager...
      When was bored with De La Cruz classes and classes and streams and streams...

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"

#include "MEMMGR.H"

#define InfoTrace(x)

int iInitCount = 0;
SHeapStatus HeapStat = {0};
CRITICAL_SECTION cSectObj = {0};
DWORD dwThreadID = 0;
bool fMultiThread = false;

#define MAX_SEG         256
#define DEF_SEGSIZE     (1024 * 1024 * 4) // 4M common heap size 1G
#define FREE_SIGN       0x46424C4B // Free block
#define USED_SIGN       0x55424C4B // Used block
#define END_SIGN        0x45424C4B // End of block
#define HEAP_GAP        255 // Minimum allocation block gap...
#define MIN_ALLOC_SIZE  8

#pragma pack(push, 1)
typedef struct _SHeapItem
{
	DWORD	dwSignature;
  DWORD dwSize;
	void *pNext;
} SHeapItem, *PSHeapItem;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _SFreeHeapItem
{
	SHeapItem hdr;
  void *pPrev; // Prev in free chain
	void *pNext; // Next in free chain
} SFreeHeapItem, *PSFreeHeapItem;
#pragma pack(pop)

void *pSegments[MAX_SEG] = {0};
int iSegCount = 0;
SFreeHeapItem *pFirstFree = 0;

static HRESULT SetHeapError(char *msg)
{
  int l = 0;
  if (msg)
  {
    InfoTrace(msg);
    l = (int)strlen(msg);
    if (l > 254) l = 254;
    memcpy(&HeapStat.cHeapError[0], msg, l);
  }
  HeapStat.cHeapError[l] = 0;
  return 0;
}

static int ExcludeFromFreeChain(SFreeHeapItem *hi)
{
   SFreeHeapItem *prev = (SFreeHeapItem *)(hi->pPrev);
   SFreeHeapItem *next = (SFreeHeapItem *)(hi->pNext);
   if ((prev == 0) && (next == 0)) { // Only the block in chain...
      pFirstFree = 0; // Drop free list pointer...
      --HeapStat.lFreeBlockCount;
      return 0;
   }
   if ((prev == 0) && (next)) { // First block in chain
     next->pPrev = 0;
     pFirstFree = next;
     --HeapStat.lFreeBlockCount;
     return 0;
   }
   if ((prev) && (next == 0)) { // Last block in chain
     prev->pNext = 0;
     --HeapStat.lFreeBlockCount;
     return 0;
   }
   if ((prev) && (next)) { // In a middle...
     prev->pNext = next;
     next->pPrev = prev;
     --HeapStat.lFreeBlockCount;
     return 0;
   }
   return E_FAIL;
}

static int IncludeToFreeChain(SFreeHeapItem *hi)
{
   if (pFirstFree == 0) { // No free blocks...
      pFirstFree = hi;
      hi->pNext = 0;
      hi->pPrev = 0;
      ++HeapStat.lFreeBlockCount;
      return 0;
   }
   // Insert as first in chain
   hi->pPrev = 0;
   hi->pNext = pFirstFree;
   pFirstFree->pPrev = hi;
   pFirstFree = hi;
   ++HeapStat.lFreeBlockCount;
   return 0;
}

// Add new memory segment...
static HRESULT AddNewSegment()
{
   if (iSegCount == MAX_SEG) {
      SetHeapError("Maximum number of segments reached");
      return E_FAIL;
   }
   DWORD s = DEF_SEGSIZE;
   void *p = malloc(s);
   if (p == 0) {
      SetHeapError("Can't allocate new heap segment");
      return E_OUTOFMEMORY;
   }
#ifdef CLEAR_MEM
   memset(p, 0, s);
#endif   
   SFreeHeapItem *hi;
   hi = (SFreeHeapItem *)p;
   hi->hdr.dwSignature = FREE_SIGN;
   hi->hdr.dwSize = s - sizeof(SHeapItem);
   hi->hdr.pNext = 0;
   hi->pNext = 0;
   hi->pPrev = 0;
   pSegments[iSegCount] = p;
   HeapStat.dwHeapUsed += sizeof(SHeapItem);
   IncludeToFreeChain(hi);
   ++iSegCount;
   HeapStat.iSegCount = iSegCount;
   HeapStat.dwSegMemory += s;
   return 0;
}

#pragma pack(push, 1)
typedef struct _SBlockStatus
{
	DWORD dwUsed;
  DWORD dwFree;
  DWORD dwMaxUsed;
  DWORD dwMaxFree;
  int iUsedCount;
  int iFreeCount;
  int iCount;
  BOOL bCanCompact;
} SBlockStatus, *PSBlockStatus;
#pragma pack(pop)

// Get block statistics
static void GetBlockStat(void *pSeg, SBlockStatus *bs)
{
   memset(bs, 0, sizeof(SBlockStatus));
   SHeapItem *hi = (SHeapItem *)pSeg;
   BOOL bLastFree = false;
   DWORD s;
   int cnt = 0;

   char buf[255];
   wsprintf(buf, "=========== Dump seg %08x =============", pSeg);
   InfoTrace(buf);

   while (true) {
     ++bs->iCount;
     ++cnt;
     if (hi->dwSignature == FREE_SIGN)
     {
       s = hi->dwSize;
       if (s > bs->dwMaxFree) bs->dwMaxFree = s;
       ++bs->iFreeCount;
       bs->dwFree += s;
       if (bLastFree) bs->bCanCompact = true;
       bLastFree = true;
       wsprintf(buf, "  %d: FREE[%08x] SIZE:%d NEXT:%08x", cnt, hi, hi->dwSize, hi->pNext);
       InfoTrace(buf);
     }
     if (hi->dwSignature == USED_SIGN)
     {
       s = hi->dwSize;
       if (s > bs->dwMaxUsed) bs->dwMaxUsed = s;
       ++bs->iUsedCount;
       bs->dwUsed += s;
       bLastFree = false;
       wsprintf(buf, "  %d: USED[%08x] SIZE:%d NEXT:%08x", cnt, hi, hi->dwSize, hi->pNext);
       InfoTrace(buf);
     }
     if (hi->pNext == 0) break;
     hi = (SHeapItem *)hi->pNext;
   }
   InfoTrace("======================================");
}

// Get block statistics
void mmDumpFreeBlocks(char *msg, BOOL bDetails)
{
   SFreeHeapItem *hi = pFirstFree;
   int cnt = 0;
   DWORD dwFree = 0;
   DWORD dwMaxFree = 0;

   char buf[255];
   if (msg) {
     wsprintf(buf, "=========== %s =============", msg);
     InfoTrace(buf);
   } else InfoTrace("=========== Dump free list =============");

   while (hi) {
     ++cnt;
     if (hi->hdr.dwSignature == FREE_SIGN)
     {
       dwFree += hi->hdr.dwSize;
       if (hi->hdr.dwSize > dwMaxFree) dwMaxFree = hi->hdr.dwSize;
       if (bDetails) {
         wsprintf(buf, "  %d: FREE[%08x] SIZE:%d NEXT:%08x FCNEXT:%08x FCPREV:%08x", cnt, hi, hi->hdr.dwSize, hi->hdr.pNext, hi->pNext, hi->pPrev);
         InfoTrace(buf);
       }
     } else {
       wsprintf(buf, "  %d: INVALID FREE BLOCK[%08x]", cnt, hi);
       InfoTrace(buf);
     }
     hi = (SFreeHeapItem *)hi->pNext;
   }
   wsprintf(buf, "====== Total free: %d, Max block: %d ===============", dwFree, dwMaxFree);
   InfoTrace(buf);
}

// Split free block onto two blocks if needed
static HRESULT SplitFreeBlock(SHeapItem *hi, DWORD s)
{
   if ((hi->dwSize - s) >= (sizeof(SHeapItem) + MIN_ALLOC_SIZE)) { // Need to split free block...
     void *x = hi->pNext;
     SHeapItem *h = (SHeapItem *)((LPBYTE)hi + sizeof(SHeapItem) + s);
     h->dwSignature = FREE_SIGN;
     h->pNext = x;
     h->dwSize = hi->dwSize - s - sizeof(SHeapItem);
     IncludeToFreeChain((SFreeHeapItem *)h);
     HeapStat.dwHeapUsed += sizeof(SHeapItem);
     hi->pNext = (void *)h;
     hi->dwSize = s;
   }
   return 0;
}

// Find suitable free block
static SHeapItem *FindFreeBlock(DWORD s)
{
   SFreeHeapItem *hi = pFirstFree;
   if (hi == 0) return 0; // No free blocks at all...
   while (true) {
      if (hi->hdr.dwSignature != FREE_SIGN) // Check...
      {
         char buf[255];
         wsprintf(buf, "HEAP ERROR: Corruption detected in free list, block %08x", (void *)hi);
         SetHeapError(buf);
         return 0;
      }
      if (hi->hdr.dwSize >= s) {
        SplitFreeBlock((SHeapItem *)hi, s);
        return (SHeapItem *)hi;
      }
      if (hi->pNext == 0) break;
      hi = (SFreeHeapItem *)hi->pNext;
   }
   return 0;
}

// Allocate block in segment
static SHeapItem *AllocateSegmentBlock(void *pSeg, DWORD s, BOOL *bCanCompact)
{
   SHeapItem *hi = (SHeapItem *)pSeg;
   SHeapItem *p = 0;
   *bCanCompact = false;
   BOOL bLastFree = false;
   while (true) {
     if (hi->dwSignature == FREE_SIGN)
     {
       if (hi->dwSize >= s) { // Free element of this size or larger found...
         SplitFreeBlock(hi, s);
         p = hi;
         break;
       }
       if (bLastFree) *bCanCompact = true;
       bLastFree = true;
     } else {
       if (hi->dwSignature != USED_SIGN) // Assumed to be used...
       {
          char buf[255];
          wsprintf(buf, "HEAP ERROR: Corruption detected in segment %08x, block %08x", (void *)pSeg, (void *)hi);
          SetHeapError(buf);
          return p;
       }
       bLastFree = false;
     }
     if (hi->pNext == 0) break;
     hi = (SHeapItem *)hi->pNext;
   }
   return p;
}

static BOOL MergeWithNextBlock(SHeapItem *hi)
{
   if (hi->pNext) { // Next block available
      SHeapItem *next = (SHeapItem *)hi->pNext;
      if (next->dwSignature == FREE_SIGN) { // It is also free block, merge them
         ExcludeFromFreeChain((SFreeHeapItem *)next);
         HeapStat.dwHeapUsed -= sizeof(SHeapItem);
         void *p = next->pNext;
         hi->dwSize += sizeof(SHeapItem) + next->dwSize;
         hi->pNext = p;
         return true;
      }
   }
   return false;
}

static bool ReleaseFreeSeg(int seg, bool release)
{
   SHeapItem *hi = (SHeapItem *)(pSegments[seg]);
   if (hi == 0) return false;
   while (true) {
     if (hi->dwSignature != FREE_SIGN) return false;
     if (hi->pNext == 0) break;
     hi = (SHeapItem *)hi->pNext;
   }
   if (!release) return true;
   hi = (SHeapItem *)(pSegments[seg]);
   while (true) {
     void *next = hi->pNext;
     ExcludeFromFreeChain((SFreeHeapItem *)hi);
     HeapStat.dwHeapUsed -= sizeof(SHeapItem);
     if (next == 0) break;
     hi = (SHeapItem *)next;
   }
   free(pSegments[seg]);
   pSegments[seg] = 0;
   int p = seg+1;
   while ((p < MAX_SEG) && (pSegments[p])) {
     pSegments[p-1] = pSegments[p];
     pSegments[p] = 0;
     ++p;
   }
   --iSegCount;
   HeapStat.iSegCount = iSegCount;
   HeapStat.dwSegMemory -= DEF_SEGSIZE;
   return true;
}

// Compact heap segment
static HRESULT CompactHeapSegment(int seg, DWORD *dwMaxBlkSize)
{
   SHeapItem *hi = (SHeapItem *)pSegments[seg];
   DWORD dwMaxSize = 0;
   BOOL bStep = true;
   while (true) {
     if (hi->dwSignature == FREE_SIGN)
     {
       if (MergeWithNextBlock(hi)) bStep = false;
       if (hi->dwSize > dwMaxSize) dwMaxSize = hi->dwSize; // Find max free block size
     } else {
       if (hi->dwSignature != USED_SIGN) // Assumed to be used...
       {
          char buf[255];
          wsprintf(buf, "HEAP ERROR: Corruption detected in segment %08x, block %08x", (void *)pSegments[seg], (void *)hi);
          SetHeapError(buf);
          return E_FAIL;
       }
     }
     if (hi->pNext == 0) break;
     if (bStep) hi = (SHeapItem *)hi->pNext;
     bStep = true;
   }
   *dwMaxBlkSize = dwMaxSize;
   return 0;
}

// Allocate appropriate block size
static void *AllocateMemBlock(DWORD dwSize)
{
  void *p = 0;
  if (iInitCount == 0)
  {
     if (mmInit()) return 0;
  }
  SHeapItem *h = 0;
  DWORD s = dwSize + sizeof(DWORD);

  while (s & 0x3) ++s;
  if (s < MIN_ALLOC_SIZE) s = MIN_ALLOC_SIZE;

  h = FindFreeBlock(s);

  if (h == 0) { // Trying to compact heap and walk through segments...
/*
    mmCompactHeap();
    h = FindFreeBlock(s);
*/
    for (int i = 0; i < MAX_SEG; i++) {
      if (pSegments[i] == 0) break;
      DWORD dwAvail = 0;
      if (CompactHeapSegment(i, &dwAvail)) return 0;
      if (dwAvail >= s) {
         h = FindFreeBlock(s);
         if (h) break;
      }
    }
  }

  if (h == 0) { // Still not have anything available...
    if (AddNewSegment()) return p;
    h = FindFreeBlock(s);
    if (h == 0) return p; // Can't allocate anything else... :(
  }

  ExcludeFromFreeChain((SFreeHeapItem *)h);
  h->dwSignature = USED_SIGN;
  s = h->dwSize - sizeof(DWORD);
  p = LPBYTE(h) + sizeof(SHeapItem);
#ifdef CLEAR_MEM  
  memset(p, 0, s);
#endif  
  DWORD *dw = (DWORD *)(LPBYTE(p) + s);
  *dw = END_SIGN;
  HeapStat.dwUsed += h->dwSize;
  HeapStat.lAllocated += h->dwSize;
  ++HeapStat.lUsedBlockCount;
  return p;
}

// Check mempry pointer for correctness
static SHeapItem *CheckHeapPointer(void *p, void **pseg)
{
  BOOL bPtrOk = false;
  char buf[255];
  *pseg = 0;
  for (int i = 0; i < MAX_SEG; i++) {
    if (pSegments[i] == 0) break;
    void *pLo = pSegments[i];
    void *pHi = (LPBYTE)pLo + DEF_SEGSIZE;
    if ((p > pLo) && (p < pHi)) {
      *pseg = pSegments[i];
      bPtrOk = true;
      break;
    }
  }
  if (!bPtrOk) {
    wsprintf(buf, "HEAP ERROR: Pointer %08x is not in segments range", p);
    SetHeapError(buf);
    return 0;
  }
  SHeapItem *h = (SHeapItem *)((LPBYTE)p - sizeof(SHeapItem));
  if (h->dwSignature != USED_SIGN) {
    wsprintf(buf, "HEAP ERROR: Pointer %08x has invalid signature (%08x)", h, h->dwSignature);
    SetHeapError(buf);
    return 0;
  }
  DWORD s = h->dwSize;
  DWORD *dw = (DWORD *)(LPBYTE(p) + (s - sizeof(DWORD)));
  if (*dw != END_SIGN) {
    wsprintf(buf, "HEAP ERROR: Pointer %08x has invalid end marker (%08x)", h, *dw);
    SetHeapError(buf);
    return 0;
  }
  return h;
}

// Free allocated block
static HRESULT FreeMemBlock(void *p)
{
  if (iInitCount == 0) return E_FAIL;
  void *seg = 0;
  SHeapItem *h = CheckHeapPointer(p, &seg);
  if ((h == 0) || (seg == 0)) return E_POINTER;
  h->dwSignature = FREE_SIGN;
  IncludeToFreeChain((SFreeHeapItem *)h);
  HeapStat.dwUsed -= h->dwSize;
  HeapStat.lFreed += h->dwSize;
  --HeapStat.lUsedBlockCount;
  while (MergeWithNextBlock(h)); // Merge with all blocks after
  return 0;
}

DWORD WINAPI mmGetMemUsed()
{
  return HeapStat.dwUsed;
}

HRESULT WINAPI mmInit()
{
   HRESULT r = 0;
   if (iInitCount > 0) {
     EnterCriticalSection(&cSectObj);
     ++iInitCount;
     LeaveCriticalSection(&cSectObj);
     return r;
   }
   InitializeCriticalSection(&cSectObj);
   EnterCriticalSection(&cSectObj);
   dwThreadID = 0;
   fMultiThread = false;
   memset(&HeapStat, 0, sizeof(SHeapStatus));
   memset(&pSegments, 0, sizeof(pSegments));
   iSegCount = 0;
   pFirstFree = 0;
   r = AddNewSegment();
   iInitCount = 1;
   LeaveCriticalSection(&cSectObj);
   return r;
}

HRESULT WINAPI mmClose()
{
   if (iInitCount == 0) return 1;
   EnterCriticalSection(&cSectObj);
   --iInitCount;
   LeaveCriticalSection(&cSectObj);
   if (iInitCount) return 1;
   EnterCriticalSection(&cSectObj);
   for (int i = 0; i < MAX_SEG; i++) {
     if (pSegments[i]) free(pSegments[i]);
   }
   memset(&HeapStat, 0, sizeof(SHeapStatus));
   memset(&pSegments, 0, sizeof(pSegments));
   iSegCount = 0;
   pFirstFree = 0;
   LeaveCriticalSection(&cSectObj);
   DeleteCriticalSection(&cSectObj);
   return 0;
}

static bool CheckMTState()
{
  DWORD thid = GetCurrentThreadId();
  if (dwThreadID) {
    fMultiThread = (dwThreadID != thid);
  } else {
    dwThreadID = thid;
  }
  return fMultiThread; 
}

void *WINAPI mmCheckMemPtr(void *p)
{
   if (iInitCount == 0) return 0;
   if (!fMultiThread) CheckMTState();
   bool flock = fMultiThread;
   if (flock) EnterCriticalSection(&cSectObj);
   void *seg;
   SHeapItem *h = CheckHeapPointer(p, &seg);
   if (seg == 0) h = 0;
   if (flock) LeaveCriticalSection(&cSectObj);
   return (void *)h;
}

void *WINAPI mmAlloc(DWORD cbSize)
{
   if (iInitCount == 0) {
      if (mmInit()) return 0;
   };
   if (!fMultiThread) CheckMTState();
   bool flock = fMultiThread;
   if (flock) EnterCriticalSection(&cSectObj);
   void *p = AllocateMemBlock(cbSize);
   if (flock) LeaveCriticalSection(&cSectObj);
   return p;
}

HRESULT WINAPI mmFree(void *p)
{
   if (iInitCount == 0) return E_FAIL;
   if (!fMultiThread) CheckMTState();
   bool flock = fMultiThread;
   if (flock) EnterCriticalSection(&cSectObj);
   HRESULT r = FreeMemBlock(p);
   if (r == S_OK) {
     ++HeapStat.iFreeCount;
     if (HeapStat.iFreeCount >= 100) {
       if (HeapStat.iCurFreeSeg == MAX_SEG) HeapStat.iCurFreeSeg = 0;
       if (pSegments[HeapStat.iCurFreeSeg]) ReleaseFreeSeg(HeapStat.iCurFreeSeg, true);
       ++HeapStat.iCurFreeSeg;
       HeapStat.iFreeCount = 0;
     }
   }
   if (flock) LeaveCriticalSection(&cSectObj);
   return r;
}

HRESULT WINAPI mmGetHeapStatus(SHeapStatus *p)
{
   if (iInitCount == 0) return E_FAIL;
   EnterCriticalSection(&cSectObj);
   memcpy(p, &HeapStat, sizeof(SHeapStatus));
   LeaveCriticalSection(&cSectObj);
   return 0;
}

// Compact heap free blocks
HRESULT WINAPI mmCompactHeap()
{
   if (iInitCount == 0) return E_FAIL;
   HRESULT r = 0;
   EnterCriticalSection(&cSectObj);
   int i = 0;
   while (i < MAX_SEG) {
     if (pSegments[i] == 0) break;
     DWORD dwAvail = 0;
     if (CompactHeapSegment(i, &dwAvail)) {
        r = E_FAIL;
        break;
     };
     if (!ReleaseFreeSeg(i, true)) ++i;
   }
   LeaveCriticalSection(&cSectObj);
   return r;
}

void *__cdecl operator new[] (size_t size) { return mmAlloc((DWORD)size); };
void *__cdecl operator new(size_t size) { return mmAlloc((DWORD)size); };

void __cdecl operator delete[] (void *p) { if (p) mmFree(p); };
void __cdecl operator delete(void *p) { if (p) mmFree(p); };

//
// MEMMGR.CPP EOF
//