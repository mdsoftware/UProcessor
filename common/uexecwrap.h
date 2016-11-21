/******************************************************************

  File Name 
      UEXECWRAP.H

  Description
      Wrapper for script execution.

  Author
      Denis M
*******************************************************************/

#ifndef _UEXECWRAP_H
#define _UEXECWRAP_H

#include <WINDOWS.H>
#include "uscript.h"

// Execution flags (continuation of MDSF_xxx)
#define MDSF_COMPILE        0x00000001 // No execute of the script, just compile
#define MDSF_USECODE        0x00000002

HRESULT WINAPI mdsExecScript(DWORD flags, HWND hwnd, DWORD userctx, 
  char *fname, USUAL *globals = 0, 
  ProcessFuncCall *fncall = 0, 
  ProcessVarAccess *varacc = 0,
  ProcessCallBack *cb = 0,
  ProcessDbgHook *dbghook = 0);

void *WINAPI mdsCompile(DWORD flags, HWND hwnd, DWORD userctx, char *fname, ProcessCallBack *cb = 0);

void mdsSetDlgInst(HANDLE hinst);

#endif

