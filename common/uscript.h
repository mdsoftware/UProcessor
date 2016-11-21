/******************************************************************

  File Name 
      SCRIPT.H

  Description
      Compiling and executing scripts.

  Author
      Denis M
*******************************************************************/

#ifndef _USCRIPT_H
#define _USCRIPT_H

#include <WINDOWS.H>
#include <stdio.h>
#include "ulists.h"

#define CB_ERROR    1
#define CB_MESSAGE  2
#define CB_INFO     3
#define CB_REFRESH  4
#define CB_REPEAT   5 // For this call dwParam is USUAL * passed as
                      // globals in mdsExecute. Must return S_OK if repeat and
                      // another value if not.

// Process callback for different processed
typedef HRESULT CALLBACK ProcessCallBack(int iCommand, DWORD dwContext, char *pszMessage, DWORD dwParam);

#define ERRMSG_MAX  511

#pragma pack(push, 1)
typedef struct _EXECCONTEXT
{
  DWORD UserCtx; // User context
  HWND hParent; // Interface parent (if specified)
  CUsualRecord *Globals; // List of global vars (0 if not specified)
  CUsualRecord *Locals; // List of locals (0 if not created yet)
  char cErrorMsg[ERRMSG_MAX+1]; // Place to leave error message
  USUAL *StackTop; // Pointer to top stack
  int StackSize; // Size of stack
  int Line; // Source line (if available)
  char *Source; // Source name (if available)
} EXECCONTEXT, *PEXECCONTEXT;
#pragma pack(pop)

//*********************************************************
// Used to call procedure and function
// ctx - pointer to execution context
// inst - instance (for methods), 0 if function
// result - result for function/method, contain NIL USUAL
// when called
// sym - name of procedure/function in LOWERCASE
// parcnt - number of parameters
// parlst - pointer to USUAL array which contain
// parameters
//*********************************************************
typedef HRESULT CALLBACK ProcessFuncCall(EXECCONTEXT *ctx, USUAL *inst, USUAL *result, char *sym, int parcnt, USUAL *parlst);

//*********************************************************
// Used to access variables, member or array item
// ctx - pointer to execution context
// cmd - ITEM_SET or ITEM_GET 
// inst - instance (for members and member arrays), 0 for variables
// item - if SET then item to set, if GET then item to get
// sym - symbolic name
// indexcnt - count of index members
// indexlst - list of array indexes
// Usage:
// For vars: ProcessVarAccess(ctx, ITEM_SET/ITEM_GET, 0, 
//                               item, varname, 0, 0)
// And so on...
//*********************************************************
typedef HRESULT CALLBACK ProcessVarAccess(EXECCONTEXT *ctx, int cmd, USUAL *inst, USUAL *item, char *sym, int indexcnt, USUAL *indexlst);

#define DBGE_INIT       1
#define DBGE_NEWSRCLINE 2
#define DBGE_NEWOPCODE  3
#define DBGE_ERROR      4
#define DBGE_CLOSE      5

//*********************************************************
// Debug hook for a process
//*********************************************************
typedef HRESULT CALLBACK ProcessDbgHook(EXECCONTEXT *ctx, int cmd, DWORD param);

#pragma pack(push, 1)
typedef struct _MDS_HEADER
{
	DWORD dSize;
	DWORD dSignature; // 0x4353444D	
	DWORD dCodeOfs;
	DWORD dSymTblOfs;
	DWORD dDebugTblOfs;
	DWORD dDebugTblLen;
	DWORD dSrcListOfs;
	DWORD dReserved3;
} MDS_HEADER, *PMDS_HEADER;
#pragma pack(pop)

// Used to execute scripts...
HRESULT WINAPI mdsExecute(HWND hwnd, DWORD userctx, MDS_HEADER *code, USUAL *globals = 0, 
  ProcessCallBack *cb = 0, 
  ProcessFuncCall *fncall = 0, 
  ProcessVarAccess *varacc = 0,
  ProcessDbgHook *dbghook = 0);
  
#define MDSF_DEBUGINFO			0x00010000
#define MDSF_OEMSOURCE			0x00020000
#define MDSF_LISTING				0x00040000
#define MDSF_NOWAIT				  0x00080000
#define MDSF_NOWND					0x00100000
#define MDSF_REPEAT					0x00200000
#define MDSF_DUMP 					0x00400000
#define MDSF_AUTOSTDCALL    0x10000000
#define MDSF_STDFLGMASK     0xffff0000

#define CDEF_BREAK				0x0001
#define CDEF_HOLD					0x0002
#define CDEF_RUN					0x0004

#define MDSH_SIGN		0x4353444D

#define MAX_SYMBOL  63

#define RET_FAILED (0xffffffff)

#pragma pack(push, 1)
typedef struct _VARSTACKITEM
{
  int Type; // VS_TYPE
  char VType[MAX_SYMBOL+1]; // VS_VTYPE
  char Name[MAX_SYMBOL+1]; // VS_NAME
  char *Str;
  int iOpCode;
  int iParam;
} VARSTACKITEM, *PVARSTACKITEM;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _OPSTACKITEM
{
  int Priority; // OS_PRIOR
  int Code; // OS_KOD
  int Ref;
} OPSTACKITEM, *POPSTACKITEM;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _FIXUPITEM
{
  int PC;
  int Type;
} FIXUPITEM, *PFIXUPITEM;
#pragma pack(pop)

#define MAX_FIXUP   16

#pragma pack(push, 1)
typedef struct _CSTACKITEM
{
  int Type; // CS_TYPE
  int Start[3]; // CS_START
  int FixupCnt; // CS_FIXUP
  FIXUPITEM FixupList[MAX_FIXUP];
} CSTACKITEM, *PCSTACKITEM;
#pragma pack(pop)

#define MAX_LINE      4095
#define MAX_SOURCE    8

#pragma pack(push, 1)
typedef struct _SOURCEITEM
{
  char FileName[_MAX_PATH];
  CSrcReader *Reader;
} SOURCEITEM, *PSOURCEITEM;
#pragma pack(pop)

typedef struct _BUFFERITEM
{
  BYTE *pBuf;
  DWORD dwPos;
  DWORD dwSize;
} BUFFERITEM, *PBUFFERITEM;

#pragma pack(push, 1)
typedef struct _DEBUGINFOITEM
{
  unsigned short Line;
  unsigned short SrcNameOffset;
  DWORD PC;
} DEBUGINFOITEM, *PDEBUGINFOITEM;
#pragma pack(pop)

bool mdsDisassemble(char *lstname, BYTE *code, BYTE *symbols, DEBUGINFOITEM *dbginfo, BYTE *srclist);
HRESULT StdFuncImpl(USUAL *result, char *sym, int parcnt, USUAL *parlst, char *errmsg);

class CMDCompiler
{
private:
    HANDLE hcLstFile;

    VARSTACKITEM *acVarStack;
    OPSTACKITEM *acOpStack;
    CSTACKITEM *acCStack;
    SOURCEITEM *acSourceLst;
    int ncSource;
    
    BUFFERITEM pcCodeBuf;
    BUFFERITEM pcSymBuf;
    BUFFERITEM pcSrcList;
    DEBUGINFOITEM *pDebugInfo;
    
    int ncError;
    bool lcClear;
    int ncCSP;
    int ncOSP;
    int ncVSP;
    int ncLType;
    int ncStrInfo;
    int ncData;
    int ncLPC;
    char ccLLst[1024];
    USUAL ucLItem;
    bool lList;
    int ncLineTotal;
    int icALastOp;
    int icAParam;
    char ccASymbol[MAX_SYMBOL+1];
    ProcessCallBack *pCallBack;
    DWORD dwCBCtx;
    bool lStackClear;
    
    DWORD dwBufLen;
    BYTE *pCode;
    
    HWND hWnd;
    
private:
    bool BufAdd(BUFFERITEM *buf, void *data, DWORD datalen);
    bool BufAlloc(BUFFERITEM *buf, DWORD size);
    DWORD AddBufSymbol(BUFFERITEM *buf, char *sym);
    void RaiseError(int nC, char *cT);    
    bool WriteLst(char *cS);
    void Lex(int nType, char *cLex, int nLex, int *cCommand);
    void OpStatus(char *cOp, int *nCode, int *nPrior);
    void PlaceOp(unsigned short dOp);
    void PlaceOffset(DWORD dwOffset);
    void PutOp(int nCode, int nPriority, char *cOp);
    void DropTo(int nPrior);
    void DropVar();
    DWORD PlaceSymbol(char *sOp, int nL);
    void PutSymbol(char *cSymbol);
    void PutConst(char *cType, BYTE *cContent);
    void ViewStatus();
    void OpenSource(char *srcfile);
    
public:
    // use mmFree to free the detached code buffer
    MDS_HEADER *DetachCode();
    DWORD Compile(HWND hwnd, char *filename, DWORD dFlg, ProcessCallBack *cb = 0, DWORD cbctx = 0);
    virtual bool StdErrMsg(int nCode, char *cText);
    virtual void NotifyExpr(char *cE);
    
public:
    CMDCompiler();
    virtual ~CMDCompiler();

};

/******************************************************
Compiled code holder.
*******************************************************/
class CCompiledCode: public CUsualObject
{
public:
  // Item functions to operate Items
  // accordingly with it's index and action (ITEM_XXX)
  // result is used only with UTEM_GET
  HRESULT ItemI(int action, int index, USUAL *item = 0);
  HRESULT ItemS(int action, char *index, USUAL *item = 0);
  HRESULT ItemU(int action, USUAL *index, USUAL *item = 0);
  HRESULT FirstItem(USUAL *index, USUAL *item);
  HRESULT NextItem(USUAL *index, USUAL *item);
  HRESULT CallMethod(char *name, USUAL *result, int parcnt, USUAL *parlst);
  HRESULT Sort(USUAL *param, bool desc = false);
  int Length();
  
public:
  CCompiledCode(MDS_HEADER *code);
  ~CCompiledCode();
protected:
};

#endif

