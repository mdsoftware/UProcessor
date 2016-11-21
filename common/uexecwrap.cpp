/******************************************************************

  File Name 
      UEXECWRAP.CPP

  Description
      Wrapper for script execution.

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"
#include "uexecwrap.h"
#include "resource1.h"

static HANDLE hInst = 0;

void mdsSetDlgInst(HANDLE hinst)
{
  hInst = hinst;
}

typedef struct _MDS_STDEXECCTX
{
	DWORD dwFlags;
	HWND hWnd;
	HWND hDlg;
	DWORD dwUserCtx;
  char cFileName[_MAX_PATH];
  MDS_HEADER *pCode;
  USUAL *uGlobals;
  ProcessFuncCall *pFuncCall; 
  ProcessVarAccess *pVarAcc;
  ProcessCallBack *pCallBack;
  ProcessCallBack *pOrigCallBack;
  ProcessDbgHook *pDbgHook;
  HRESULT Result;
} MDS_STDEXECCTX, *PMDS_STDEXECCTX;

static HRESULT CALLBACK _StdCallBack(int iCommand, DWORD dwContext, char *pszMessage, DWORD dwParam)
{
  if (iCommand == CB_MESSAGE) {
    // MessageBox(0, pszMessage, "Message", MB_OK | MB_TASKMODAL | MB_ICONINFORMATION);
    return S_OK;
  }
  else if (iCommand == CB_ERROR) {
    MessageBox(0, pszMessage, "Error", MB_OK | MB_TASKMODAL | MB_ICONERROR);
    return S_OK;
  }
  else if (iCommand == CB_INFO) {
    HWND hTxt = GetDlgItem((HWND)dwParam, IDC_STATUSMSG);
    if (hTxt) {
      SetWindowText(hTxt, pszMessage);
      UpdateWindow((HWND)dwParam);
    }
  }
  else if (iCommand == CB_REFRESH) {
    HWND hPBar = GetDlgItem((HWND)dwParam, IDC_PROGRESS);
    if (hPBar) {
      int p = SendMessage(hPBar, PBM_GETPOS, 0, 0);
      ++p;
      if (p >= 100) p = 0;
      SendMessage(hPBar, PBM_SETPOS, p, 0);  
    }
    MSG msg = {0};
	  while (PeekMessage(&msg, 0, 0, 0xFFFFFFFF, PM_REMOVE)) {
		  TranslateMessage(&msg);
		  SendMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
	  }
  }
  return S_OK;
}

void *WINAPI mdsCompile(DWORD flags, HWND hwnd, DWORD userctx, char *fname, ProcessCallBack *cb)
{
  MDS_HEADER *code = 0;
	CMDCompiler *cmp = new CMDCompiler();
  DWORD res = cmp->Compile(hwnd, fname, 
     flags, cb, userctx);
  if ((res & 0x80000000) == 0) code = cmp->DetachCode();
  delete cmp;
  cmp = 0;
  return (void *)code;
}

static void _ScriptRunExec(MDS_STDEXECCTX *ctx)
{
  MDS_HEADER *code = 0;
  if (ctx->dwFlags & MDSF_USECODE) {
    code = (MDS_HEADER *)ctx->pCode;
  } else {
	  CMDCompiler *cmp = new CMDCompiler();
	  if (ctx->hDlg) SetWindowText(ctx->hDlg, "Compilation");
    DWORD res = cmp->Compile(ctx->hWnd, ctx->cFileName, 
      ctx->dwFlags & MDSF_STDFLGMASK, ctx->pCallBack, ctx->dwUserCtx);
    if ((res & 0x80000000) == 0) code = cmp->DetachCode();
    delete cmp;
    cmp = 0;
  }
  if (code) {
     int cnt = 0;
     while (true) {
       ++cnt;
       if (ctx->hDlg) {
          char b[128];
          sprintf(b, "Execution (%d)", cnt);
          SetWindowText(ctx->hDlg, b);
          HWND hTxt = GetDlgItem(ctx->hDlg, IDC_STATUSMSG);
          if (hTxt) SetWindowText(hTxt, ctx->cFileName);
          UpdateWindow(ctx->hDlg);
       }
       ctx->Result = mdsExecute(ctx->hDlg, ctx->dwUserCtx, code,
          ctx->uGlobals,
          ctx->pCallBack, 
          ctx->pFuncCall, 
          ctx->pVarAcc,
          ctx->pDbgHook);
       if ((ctx->dwFlags & MDSF_REPEAT) == 0) break;
       if (ctx->pOrigCallBack == 0) break;
       if (ctx->pOrigCallBack(CB_REPEAT, ctx->dwUserCtx, 0, (DWORD)ctx->uGlobals)) break;
     }
     if ((ctx->dwFlags & MDSF_USECODE) == 0) mmFree(code);
  } else {
     ctx->Result = E_FAIL;
  }
}

#define WM_CUSTINIT (WM_USER + 100)

static LRESULT CALLBACK _StdDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	  SetWindowLong(hDlg, GWL_USERDATA, (LONG)lParam);
		return TRUE;
		
  case WM_SHOWWINDOW:
    {
    HWND hPBar = GetDlgItem(hDlg, IDC_PROGRESS);
    if (hPBar) {
      SendMessage(hPBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));  
      SendMessage(hPBar, PBM_SETPOS, 0, 0);  
    }
    PostMessage(hDlg, WM_CUSTINIT, 0, 0);
    }
    break;
		
	case WM_CUSTINIT:
	  if (wParam) {
	    SetFocus(hDlg);
	    MDS_STDEXECCTX *ctx = (MDS_STDEXECCTX *)GetWindowLong(hDlg, GWL_USERDATA);
	    ctx->hWnd = hDlg;
	    ctx->hDlg = hDlg;
	    _ScriptRunExec(ctx);
      EndDialog(hDlg, 0);
	  } else {
	    PostMessage(hDlg, WM_CUSTINIT, 1, 1);
	  }
	  return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			return TRUE;
		}
		break;
	}
	return FALSE;
}

HRESULT WINAPI mdsExecScript(DWORD flags, HWND hwnd, DWORD userctx, 
  char *fname, USUAL *globals, 
  ProcessFuncCall *fncall, 
  ProcessVarAccess *varacc,
  ProcessCallBack *cb,
  ProcessDbgHook *dbghook)
{
  MDS_STDEXECCTX ctx = {0};
  ctx.dwFlags = flags;
  if (ctx.dwFlags & MDSF_USECODE) {
    ctx.pCode = (MDS_HEADER *)fname;
    if (ctx.pCode->dSignature != MDSH_SIGN) return E_INVALIDARG;
    sprintf(ctx.cFileName, "Code, size %d", ctx.pCode->dSize);
  } else {
    strcpy(ctx.cFileName, fname);
  }
  ctx.dwUserCtx = userctx;
  ctx.hWnd = hwnd;
  ctx.pCallBack = 0;
  ctx.pDbgHook = dbghook;
  ctx.pFuncCall = fncall;
  ctx.pVarAcc = varacc;
  ctx.pOrigCallBack = cb;
  ctx.Result = S_OK;
  ctx.uGlobals = globals;
  ctx.hDlg = 0;
  if (ctx.dwFlags & MDSF_NOWND) {
    ctx.pCallBack = cb;
    ctx.pDbgHook = dbghook;
    _ScriptRunExec(&ctx);
    return ctx.Result;
  }
  ctx.pCallBack = &_StdCallBack;
  int dlgres = DialogBoxParam((HINSTANCE)hInst, (LPCTSTR)IDD_STDEXECDLG, ctx.hWnd, (DLGPROC)_StdDlgProc, (LONG)&ctx);
  if (dlgres == -1) {
    ctx.Result = GetLastError();
    if (cb) {
       char b[255];
       sprintf(b, "Dialog box error: 0x%08x", ctx.Result);
       MessageBox(0, b, "Dialog Box Error", MB_OK | MB_TASKMODAL);
    }
  }
  return ctx.Result;
}

//
// uexecwrap.cpp EOF
//

