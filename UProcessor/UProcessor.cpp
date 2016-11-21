// UProcessor.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

HANDLE __hInst = 0;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
      __hInst = hModule;
      mdsSetDlgInst(hModule);
      mmInit();
      return TRUE;
    } else if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
      if (mmGetMemUsed()) {
        char b[128];
        sprintf(b, "Some memory (%d) left allocated while closing memory manager", mmGetMemUsed());
        MessageBox(0, b, "Memory Error", MB_OK | MB_TASKMODAL | MB_ICONERROR);
      }
      mmClose();
      __hInst = 0;
      return TRUE;
    }
    return TRUE;
}

//
// Public DLL functions
//

// Memory management
void *WINAPI UMemAlloc(DWORD size)
{
  return mmAlloc(size);
}

HRESULT WINAPI UMemFree(void *p)
{
  return mmFree(p);
}

DWORD WINAPI UMemUsed()
{
  return mmGetMemUsed();
}

HRESULT WINAPI UMemCompact()
{
  return mmCompactHeap();
}

HRESULT WINAPI UMemInfo(SHeapStatus *info)
{
  return mmGetHeapStatus(info);
}

// FastDate support
HRESULT WINAPI UPackDate(int year, int month, int day, FASTDATE *date)
{
  return fdPackDate(year, month, day, date);
}

HRESULT WINAPI UPackTime(int hour, int min, int sec, FASTDATE *date)
{
  return fdPackTime(hour, min, sec, date);
}

HRESULT WINAPI UUnpackDate(FASTDATE *src, UNPACKEDDATE *dest)
{
  return fdUnpackDate(src, dest);
}

bool WINAPI UIsLeap(FASTDATE *d)
{
  return fdIsLeap(d);
}

int WINAPI UDayOfWeek(FASTDATE *d)
{
  return fdDayOfWeek(d);
}

int WINAPI UCmpDates(FASTDATE *x, FASTDATE *y)
{
  return fdCmpDates(x, y);
}

HRESULT WINAPI UAddDate(FASTDATE *result, FASTDATE *x, FASTDATE *y)
{
  return fdAddDate(result, x, y);
}

HRESULT WINAPI USubDate(FASTDATE *result, FASTDATE *x, FASTDATE *y)
{
  return fdSubDate(result, x, y);
}

HRESULT WINAPI UCurDate(FASTDATE *d)
{
  return fdCurDate(d);
}


// USUAL processing
HRESULT WINAPI UClear(USUAL *var);
HRESULT WINAPI UAssign(USUAL *dest, USUAL *src);
HRESULT WINAPI USetStrA(USUAL *var, char *str, int len);
HRESULT WINAPI USetStrW(USUAL *var, WCHAR *str, int len);
HRESULT WINAPI USetInt(USUAL *var, long value);
HRESULT WINAPI USetFloat(USUAL *var, double value);
HRESULT WINAPI USetLogic(USUAL *var, bool value);
HRESULT WINAPI USetDate(USUAL *var, FASTDATE *value);

HRESULT WINAPI UNewObject(USUAL *var, int objtype, int initsize, void *data = 0)
{
  CUsualObject *obj = 0;
  switch (objtype) {
    case UCT_RECORD:
      {
        CUsualRecord *rec = new CUsualRecord(initsize);
        obj = rec;
        break;
      }  
    case UCT_ARRAY:
      {
        CUsualArray *arr = new CUsualArray(initsize);
        obj = arr;
        break;
      }
    case UCT_CODE:
      {
        CCompiledCode *code = new CCompiledCode((MDS_HEADER *)data);
        obj = code;
        break;
      }  
  }
  if (obj == 0) return E_FAIL;
  HRESULT r = USetObj(var, obj);
  if (r) {
    delete obj;
  }
  return r;
}

void *WINAPI UObjData(USUAL *var)
{
  if (var->ulTag != MAKE_TAG(UT_OBJECT)) return 0;
  if (var->Value.obj == 0) return 0;
  return var->Value.obj->Data();
}

HRESULT WINAPI UCompare(USUAL *x, USUAL *y, int *result);
HRESULT WINAPI UIsTrue(USUAL *x, bool *result);

#define UOP_ADD       1
#define UOP_SUB       2
#define UOP_MUL       3
#define UOP_DIV       4
#define UOP_NEG       5
#define UOP_AND       6
#define UOP_OR        7
#define UOP_NOT       8

HRESULT WINAPI UOperator(int opcode, USUAL *result, USUAL *x, USUAL *y)
{
  switch (opcode) {
    case UOP_ADD:
      return UAdd(result, x, y);
    case UOP_SUB:
      return USub(result, x, y);
    case UOP_MUL:
      return UMul(result, x, y);
    case UOP_DIV:
      return UDiv(result, x, y);
    case UOP_NEG:
      return UNeg(result, x);
    case UOP_AND:
      return UAnd(result, x, y);
    case UOP_OR:
      return UOr(result, x, y);
    case UOP_NOT:
      return UNot(result, x);
  }
  return E_INVALIDARG;
}

int WINAPI UTypeOf(USUAL *var)
{
  if ((var->ulTag & USUAL_SIGN_MASK) != USUAL_SIGN) return -1;
  return TAG_TYPE(var->ulTag);
}

HRESULT WINAPI USetCargo(USUAL *var, unsigned long cargo)
{
  if ((var->ulTag & USUAL_SIGN_MASK) != USUAL_SIGN) return E_FAIL;
  var->ulCargo = cargo;
  return S_OK;
}

unsigned long WINAPI UGetCargo(USUAL *var)
{
  if ((var->ulTag & USUAL_SIGN_MASK) != USUAL_SIGN) return 0;
  return var->ulCargo;
}

char *WINAPI UGetRecFldName(USUAL *rec, int index)
{
  if (rec->ulTag != MAKE_TAG(UT_OBJECT)) return 0;
  if (rec->Value.obj == 0) return 0;
  if (rec->Value.obj->Type() != UCT_RECORD) return 0;
  return ((CUsualRecord *)(rec->Value.obj))->GetFldName(index);
}

static void CopySym(char *dest, char *sym)
{
  int l = strlen(sym);
  if (l > MAX_SYMBOL) l = MAX_SYMBOL;
  memcpy(dest, sym, l);
  dest[l] = 0;
  _strlwr(dest);
}

// Object and functions handling
HRESULT WINAPI UStrFuncCall(USUAL *result, char *sym, int parcnt, USUAL *parlst)
{
  char s[MAX_SYMBOL+1];
  CopySym(s, sym);
  return StdFuncImpl(result, s, parcnt, parlst, 0);
}

HRESULT WINAPI UCallMethod(USUAL *inst, USUAL *result, char *sym, int parcnt, USUAL *parlst)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_FAIL;
  char s[MAX_SYMBOL+1];
  CopySym(s, sym);
  return inst->Value.obj->CallMethod(s, result, parcnt, parlst);
}

HRESULT WINAPI UGetItem(USUAL *inst, USUAL *index, USUAL *item)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_FAIL;
  return inst->Value.obj->ItemU(ITEM_GET, index, item);
}

HRESULT WINAPI USetItem(USUAL *inst, USUAL *index, USUAL *item)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_FAIL;
  return inst->Value.obj->ItemU(ITEM_SET, index, item);
}

HRESULT WINAPI URemoveItem(USUAL *inst, USUAL *index)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_FAIL;
  return inst->Value.obj->ItemU(ITEM_REMOVE, index, 0);
}

int WINAPI ULen(USUAL *var)
{
  if (var->ulTag == MAKE_TAG(UT_OBJECT)) {
    if (var->Value.obj) return var->Value.obj->Length();
    return 0;
  } else if (var->ulTag == MAKE_TAG(UT_STR_A)) {
    return var->Value.strA.len;
  } else if (var->ulTag == MAKE_TAG(UT_STR_W)) {
    return var->Value.strW.len;
  } else if (var->ulTag == MAKE_TAG(UT_SYMBOL)) {
    if (var->Value.sym) return strlen(var->Value.sym);
    return 0;
  }
  return -1;
}

int WINAPI UObjType(USUAL *var)
{
  if (var->ulTag == MAKE_TAG(UT_OBJECT)) {
    if (var->Value.obj) return var->Value.obj->Type();
    return 0;
  }
  return -1;
}

HRESULT WINAPI UFirstItem(USUAL *inst, USUAL *index, USUAL *item)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_INVALIDARG;
  return inst->Value.obj->FirstItem(index, item);
}

HRESULT WINAPI UNextItem(USUAL *inst, USUAL *index, USUAL *item)
{
  if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return E_INVALIDARG;
  if (inst->Value.obj == 0) return E_INVALIDARG;
  return inst->Value.obj->NextItem(index, item);
}

#pragma pack(push, 1)
typedef struct _UExecParam
{
  DWORD flags;
  HWND hwnd;
  DWORD userctx; 
  char *fname;
  USUAL *globals;
  ProcessFuncCall *fncall;
  ProcessVarAccess *varacc;
  ProcessCallBack *cb;
  ProcessDbgHook *dbghook;
} UExecParam, *PUExecParam;
#pragma pack(pop)

// Executing of the scripts
HRESULT WINAPI UExecute(UExecParam *par)
{
  return mdsExecScript(par->flags, par->hwnd, par->userctx, 
    par->fname, par->globals, par->fncall, par->varacc, par->cb, par->dbghook);
}

void *WINAPI UCompile(DWORD flags, HWND hwnd, DWORD userctx, char *fname, ProcessCallBack *cb = 0)
{
  return mdsCompile(flags, hwnd, userctx, fname, cb);
}

//
// UProcessor.cpp EOF
//
