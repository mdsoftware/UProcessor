/******************************************************************

  File Name 
      USCRIPT.CPP

  Description
      Compiling and executing scripts.

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"
#include "uscript.h"

#define NULL_STRING 0


#define MAX_STACK        128
#define MAX_CSTACK       16
#define BUFFER_X         128

#define DEF_EXT         ".scr"
#define CMD_NONE         0
#define CMD_BREAK        1
#define CMD_CONT         3
	

#define XT_NONE          0
#define XT_SYMBOL        1
#define XT_STRING        2
#define XT_OPERATOR      3
#define XT_NUMBER        4
	

#define OP_PLUS          1
#define OP_MINUS         2
#define OP_UMIN       1002
#define OP_UPLUS      1003
#define OP_DIV           3
#define OP_MUL           4

#define OP_AND           10
#define OP_OR            11
#define OP_NOT         1012
#define OP_LEQ           13
#define OP_LESS          14
#define OP_GREAT         15
#define OP_LE            16
#define OP_GE            17
#define OP_NE            18	

#define UNC_SET          "(),:;[]\0"
#define MNC_SET           "+-*/=<>!\0"
#define DELIM_SET        " \0"
#define ERROR_SET        "{}\\|\0"
#define NUM_SET          "0123456789.\0"

#define DO_IF                         1
#define DO_WHILE                      2
#define OP_LPAR          100
#define OP_RPAR          101
#define OP_COMMA         102
#define OP_LFPAR         103
#define OP_LSPAR         110
#define OP_RSPAR         111
#define OP_MEMBER        120

#define OP_ASSIGN        200

#define OPC_PUSHCN        1
#define OPC_PUSHCI        2
#define OPC_PUSHCF        3
#define OPC_PUSHCC        4
#define OPC_PUSHCL        5
#define OPC_PUSHCS        6

#define OPC_PUSHV         10
#define OPC_POPV          20

#define OPC_ASSIGN        100
#define OPC_ADD           101
#define OPC_SUB           102
#define OPC_MUL           103
#define OPC_DIV           104
#define OPC_UADD          105
#define OPC_USUB          106
#define OPC_ARR_GET       107
#define OPC_ARR_SET       108
#define OPC_ARR_MEMBER_GET  109
#define OPC_ARR_MEMBER_SET  110
#define OPC_MEMBER_GET    111
#define OPC_MEMBER_SET    112

#define OPC_NOT           150
#define OPC_AND           151
#define OPC_OR            152
#define OPC_EQ            153
#define OPC_LESS          154
#define OPC_GREAT         155
#define OPC_LE            156
#define OPC_GE            157
#define OPC_NE            158

#define OPC_JUMP          200
#define OPC_JT            201
#define OPC_JF            202
#define OPC_CALLS         203
#define OPC_CALLN         204
#define OPC_METHOD        207

#define OPC_QUIT          300
#define OPC_NOP           301
#define OPC_BREAK         302

#define OPC_STDOUT        400

#define  VS_TYPE      1
#define  VS_VTYPE     2
#define  VS_NAME      3

#define  OS_PRIOR     1
#define  OS_KOD       2
#define  CS_TYPE      1
#define  CS_START     2
#define  CS_FIXUP     3

#define CDEP_MAXBP		32
#define MAX_ESTACK    64

#define MAX_BUFFER      0x200000 // 2M
#define MAX_SRCLINE     50000

#define MDS_EXC_SIGNATURE "MDSCompiler Exception"

#define MAX_EXECSTACK 128
#define STACK_LEFT (MAX_EXECSTACK - ctx.SP)
#define PSTACK_LEFT (MAX_EXECSTACK - ctx->SP)

typedef struct _INTERNALEXECCONTEXT
{
  EXECCONTEXT Ctx;
  int SP;
  USUAL Stack[MAX_EXECSTACK];
  DWORD PC;
  bool Debug;
  BYTE *Code;
  BYTE *Symbols;
  DEBUGINFOITEM *DebugInfo;
  DEBUGINFOITEM LastDbgInfo;
  int DbgInfoCnt;
  BYTE *SrcList;
  ProcessCallBack *CallBack;
  ProcessFuncCall *FuncCall;
  ProcessVarAccess *VarAccess;
  ProcessDbgHook *DbgHook;
} INTERNALEXECCONTEXT, *PINTERNALEXECCONTEXT;

static bool SymCmp(char *sym, char *str);

static int FmtDate(FASTDATE *d, char *buf);

static int FmtUsual(USUAL *x, char *buf, int buflen)
{
  buf[0] = 0;
  switch (TAG_TYPE(x->ulTag)) {
    case UT_INT:
      return sprintf(buf, "%d", x->Value.l, x->Value.l);
    case UT_DOUBLE:
      return sprintf(buf, "%f", x->Value.dbl);
    case UT_NIL:
      return strlen(strcpy(buf, "NIL"));
    case UT_OBJECT:
      return sprintf(buf, "OBJECT:0x%08x", (void *)x->Value.obj);
    case UT_LOGIC:
      return sprintf(buf, "LOGIC:%d", x->Value.b);
    case UT_SYMBOL:
      return sprintf(buf, "SYMBOL:%s", x->Value.sym);      
    case UT_REF:
      return sprintf(buf, "REF:0x%08x", (void *)x->Value.ref);
    case UT_STR_A:
      {
        int l = x->Value.strA.len;
        if (l > buflen) l = buflen;
        memcpy(buf, x->Value.strA.str, l);
        buf[l] = 0;
        return l;
      }
    case UT_DATETIME:
      return FmtDate(&(x->Value.fd), buf);
  }
  return 0;
}

// Standard functions implementation
HRESULT StdFuncImpl(USUAL *result, char *sym, int parcnt, USUAL *parlst, char *errmsg)
{
  HRESULT r = E_INVALIDARG;
  bool lFound = false;
  bool lBadPar = false;
  UClear(result);
  
  if (sym[0] == 'a') {
    if (SymCmp(sym, "array")) {
      int l = 0;
      if (parcnt) {
        if (PARAM(1).ulTag == MAKE_TAG(UT_INT)) l = PARAM(1).Value.l;
      }
      CUsualArray *arr = new CUsualArray(l);
      if (arr == 0) return E_FAIL;
      return USetObj(result, arr);
    }
  
  } else if (sym[0] == 'r') {
    if (SymCmp(sym, "record")) {
      int l = 0;
      if (parcnt) {
        if (PARAM(1).ulTag == MAKE_TAG(UT_INT)) l = PARAM(1).Value.l;
      }
      CUsualRecord *rec = new CUsualRecord(l);
      if (rec == 0) return E_FAIL;
      return USetObj(result, rec);
    } else if (SymCmp(sym, "random")) {
      UClear(result);
      double rnd = (double)rand() / (double)RAND_MAX;
      result->ulTag = MAKE_TAG(UT_DOUBLE);
      result->Value.dbl = rnd;
      if (parcnt > 0) {
        if (PARAM(1).ulTag = MAKE_TAG(UT_INT)) {
          result->ulTag = MAKE_TAG(UT_INT);
          result->Value.l = (int)(rnd * (double)PARAM(1).Value.l);
        }
      }
      return S_OK;
    }
  } else if (sym[0] == 'm') {
    if (SymCmp(sym, "message")) {
      if (parcnt) {
        if (PARAM(1).ulTag == MAKE_TAG(UT_STR_A)) {
          MessageBox(0, PARAM(1).Value.strA.str, "Message", MB_OK | MB_TASKMODAL);
        }
      }
      UClear(result);
      return S_OK;
    } else if (SymCmp(sym, "memory")) {
      return USetInt(result, mmGetMemUsed());
    } else if (SymCmp(sym, "meminfostr")) {
      char buf[1024];
      buf[0] = 0;
      SHeapStatus hs = {0};
      mmGetHeapStatus(&hs);
      sprintf(buf, "Allocated:%dk SegMem:%dM UsedBlocks:%d FreeBlocks:%d", hs.dwUsed / 1024, hs.dwSegMemory / (1024 * 1024), hs.lUsedBlockCount, hs.lFreeBlockCount);
      return USetStrA(result, buf, -1);
    }
  } else if (sym[0] == 't') {
    if (SymCmp(sym, "tostring")) {
      if (parcnt) {
        char buf[2048];
        buf[0] = 0;
        FmtUsual(&(PARAM(1)), buf, sizeof(buf)-1);
        return USetStrA(result, buf);
      }
      return E_FAIL;
    }
  }
  /*

  if (sym[0] == 'a') { // At

   	  IF cFunc == "AT"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 2
   	  	    uRet := At2(aPList[1], aPList[2])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ASSTRING"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := AsString(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ASHEXSTRING"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := AsHexString(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF


      ELSEIF cFunc == "ASKYESNO"
   	  	 IF nPCnt >= 1
   	  	 	  lFound := TRUE
   	  	 	  IF nPCnt == 1
   	  	 	  	 nP := mdsMessage(aPList[1], "Question", _OR(MB_ICONQUESTION, MB_YESNO))
   	  	 	  ELSE
               nP := mdsMessage(aPList[1], aPList[2], _OR(MB_ICONQUESTION, MB_YESNO))
            ENDIF
            uRet := (nP == IDYES)
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ALLTRIM"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := AllTrim(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	
	

   	  ENDIF

   } else if (sym[0] == 'e') { // Execute

   	  IF cFunc == "EXECUTE"	
   	  	 lFound := TRUE
   	  	 IF nPCnt >= 1
   	  	 	  cE := "{|__ParList|" + aPList[1] + "("
   	  	 	  IF nPCnt > 1
   	  	 	  	 FOR ni := 2 UPTO nPCnt
   	  	 	  	 	  IF ni > 2
   	  	 	  	 	  	 cE += ","
   	  	 	  	 	  ENDIF	
   	  	 	  	 	  cE += "__ParList[" + NTrim(ni) + "]"
   	  	 	  	 NEXT
   	  	 	  ENDIF
   	  	 	  cE += ")}"
   	  	 	  uRet := Eval(&(cE), aPList)
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ERROR"
   	  	 lFound := TRUE
   	  	 IF nPCnt >= 1
   	  	 	  IF nPCnt == 1
   	  	 	  	 mdsMessage(aPList[1], "Error", _OR(MB_ICONERROR, MB_OK))
   	  	 	  ELSE
               mdsMessage(aPList[1], aPList[2], _OR(MB_ICONERROR, MB_OK))
            ENDIF
            uRet := TRUE	
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "EMPTY"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
            uRet := Empty(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "EXITCODE"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    nExitCode := aPList[1]
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF


	
   	  ENDIF

   } else if (sym[0] == 'c') { // CToD

   	  IF cFunc == "CTOD"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := CToD(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ENDIF

   } else if (sym[0] == 'l') { // LTrim

   	  IF cFunc == "LTRIM"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := LTrim(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ENDIF

   } else if (sym[0] == 'i') { // IsDefined

   	  IF cFunc == "ISDEFINED"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := oVarList:IsDefined(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ISSTRING"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := IsString(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ISLOGIC"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := IsLogic(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ISDATE"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := IsDate(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ISNUMERIC"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := IsNumeric(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "ISNIL"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := IsNil(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ENDIF	

   } else if (sym[0] == 'r') { // RTrim

   	  IF cFunc == "RTRIM"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := RTrim(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ENDIF

   } else if (sym[0] == 't') { // Transform

   	  IF cFunc == "TRANSFORM"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 2
   	  	    uRet := Transform(aPList[1], aPList[2])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "TRIM"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := Trim(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	

   	  ENDIF

   } else if (sym[0] == 'p') { // Padr, Padl

   	  IF cFunc == "PADR"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	 	  uRet := PadR(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "PADL"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	 	  uRet := PadL(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF
	
      ENDIF

   } else if (sym[0] == 'd') { // DToS, DToC

   	  IF cFunc == "DTOS"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := DToS(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "DTOC"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := DToC(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	

   	  ELSEIF cFunc == "DATE"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 0
   	  	    uRet := Today()
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	


   	  ENDIF	

   } else if (sym[0] == 'v') { // Val

   	  IF cFunc == "VAL"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := Val(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF
   	  ENDIF

   } else if (sym[0] == 'm') { // Message

   	  IF cFunc == "MESSAGE"
   	  	 lFound := TRUE
   	  	 IF nPCnt >= 1
   	  	 	  IF nPCnt == 1
   	  	 	  	 mdsMessage(aPList[1], "Message", _OR(MB_ICONINFORMATION, MB_OK))
   	  	 	  ELSE
               mdsMessage(aPList[1], aPList[2], _OR(MB_ICONINFORMATION, MB_OK))
            ENDIF
            uRet := TRUE	
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "MENUSELECT"
   	  	 lFound := TRUE
   	  	 IF nPCnt >= 1
            uRet := mdsMenu(oParent:Handle(), nPCnt, aPList)
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	

   	  ELSEIF cFunc == "MONTH"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
            uRet := Month(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF	

   	  ENDIF

   } else if (sym[0] == 'n') { // Ntrim

   	  IF cFunc == "NTRIM"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
            uRet := NTrim(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF
   	  ENDIF


   } else if (sym[0] == 'w') { // Warning

   	  IF cFunc == "WARNING"
   	  	 lFound := TRUE
   	  	 IF nPCnt >= 1
   	  	 	  IF nPCnt == 1
   	  	 	  	 mdsMessage(aPList[1], "Warning", _OR(MB_ICONWARNING, MB_OK))
   	  	 	  ELSE
               mdsMessage(aPList[1], aPList[2], _OR(MB_ICONWARNING, MB_OK))
            ENDIF
            uRet := TRUE	
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF
   	  ENDIF

   } else if (sym[0] == 's') { // SubStr, SToD, Str, Space

   	  IF cFunc == "SUBSTR"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 2
   	  	 	  uRet := SubStr2(aPList[1], aPList[2])
   	  	 ELSEIF nPCnt == 3
   	  	 	  uRet := SubStr3(aPList[1], aPList[2], aPList[3])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "STOD"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := SToD(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "SPACE"
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := Space(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ELSEIF cFunc == "STR"
	       lFound := TRUE
	       IF nPCnt == 1
	       	  uRet := Str1(aPList[1])
	       ELSEIF nPCnt == 2
	       	  uRet := Str2(aPList[1], aPList[2])
	       ELSEIF nPCnt == 3
	       	  uRet := Str3(aPList[1], aPList[2], aPLIst[3])
	       ELSE
	       	  lBadPar := TRUE
	       ENDIF
   	  ENDIF

   } else if (sym[0] == 'y') { // Year

   	  IF cFunc == "YEAR"	
   	  	 lFound := TRUE
   	  	 IF nPCnt == 1
   	  	    uRet := Year(aPList[1])
   	  	 ELSE
   	  	 	  lBadPar := TRUE
   	  	 ENDIF

   	  ENDIF

  }
*/
  if (lBadPar) {
    if (errmsg) sprintf(errmsg, "Invalid parameters calling %s(...)", sym);
    r = E_INVALIDARG;
  }
 
  return r;
}

static DEBUGINFOITEM *FindDebugInfo(DEBUGINFOITEM *info, int count, DWORD pc)
{
  if ((info == 0) || (count < 1)) return 0;
  int L = 0;
  int H = count - 1;
  while (L <= H) {
    int I = ((L + H) >> 1);
    if (pc > info[I].PC) L = I + 1; else
    {
      H = I - 1;
      if (pc == info[I].PC) {
        return &(info[I]);
      }
    };
  }
  return 0;
}

static CUsualRecord *GetInstRef(EXECCONTEXT *ctx, USUAL *inst)
{
  CUsualObject *rec = 0;
  if (inst) {
    if (inst->ulTag != MAKE_TAG(UT_OBJECT)) return 0;
    rec = inst->Value.obj;
  } else {
    if (ctx->Globals) {
      rec = ctx->Globals;
    } else {
      if (ctx->Locals == 0) {
        ctx->Locals = new CUsualRecord(16);
        if (ctx->Locals) ctx->Locals->AddRef();
      }
      rec = ctx->Locals;
    }
  }
  if (rec) {
    if (rec->Type() != UCT_RECORD) return 0;
    return (CUsualRecord *)rec;
  }
  return 0;
}

static HRESULT StdVarAccess(EXECCONTEXT *ctx, int cmd, USUAL *inst, USUAL *item, char *sym, int indexcnt, USUAL *indexlst)
{
  CUsualObject *rec = 0;
  rec = GetInstRef(ctx, inst);
  if (rec == 0) {
    sprintf(ctx->cErrorMsg, "Invalid context while access var '%s'", sym);
    return E_FAIL;
  }
  HRESULT r = S_OK;
  if (indexcnt == 0) {
     r = rec->ItemS(cmd, sym, item);
     if (r) sprintf(ctx->cErrorMsg, "Error access var '%s'", sym);
     return r;
  }
  USUAL u = {0};
  r = rec->ItemS(ITEM_GET, sym, &u);
  if (r) {
    sprintf(ctx->cErrorMsg, "Error get array ref '%s'", sym);
    return r;
  }
  int i = indexcnt;
  while (true) {
    --i;
    if (u.ulTag != MAKE_TAG(UT_OBJECT)) {
       UClear(&u);
       sprintf(ctx->cErrorMsg, "Not an object [%d] while access array '%s'", indexcnt - i, sym);
       return E_FAIL;
    };
    if (u.Value.obj == 0) {
       UClear(&u);
       sprintf(ctx->cErrorMsg, "Empty object [%d] while access array '%s'", indexcnt - i, sym);
       return E_FAIL;
    }
    if ((u.Value.obj->Type() != UCT_ARRAY) && 
        (u.Value.obj->Type() != UCT_COLLECTION)) {
       bool err = true;
       if ((u.Value.obj->Type() == UCT_RECORD) && (indexcnt == 1)) {
         if ((indexlst[i].ulTag == MAKE_TAG(UT_STR_A)) ||
             (indexlst[i].ulTag == MAKE_TAG(UT_STR_W))) err = false;
       }
       if (err) {
         UClear(&u);
         sprintf(ctx->cErrorMsg, "Not an array or collection [%d] while access array '%s'", indexcnt - i, sym);
         return E_FAIL;
       }
    }
    rec = u.Value.obj;
    UClear(&u);
    if (i == 0) break;
    r = rec->ItemU(ITEM_GET, &(indexlst[i]), &u);
    if (r) {
       sprintf(ctx->cErrorMsg, "Error get array ref [%d] from '%s'", indexcnt - i, sym);
       return E_FAIL;
    }
  }
  r = rec->ItemU(cmd, &(indexlst[0]), item);
  if (r) sprintf(ctx->cErrorMsg, "Error access array '%s'", sym);
  return r;
}

static HRESULT StdFuncCall(EXECCONTEXT *ctx, USUAL *inst, USUAL *result, char *sym, int parcnt, USUAL *parlst)
{
  if (inst) {
    if (inst->ulTag != MAKE_TAG(UT_OBJECT)) {
      sprintf(ctx->cErrorMsg, "Invalid instance calling method '%s'", sym);
      return E_FAIL;
    };
    if (inst->Value.obj == 0) {
      sprintf(ctx->cErrorMsg, "Empty object calling method '%s'", sym);
      return E_FAIL;
    }
    return inst->Value.obj->CallMethod(sym, result, parcnt, parlst);
  }
  return StdFuncImpl(result, sym, parcnt, parlst, ctx->cErrorMsg);
}

static HRESULT CallVarAccess(INTERNALEXECCONTEXT *ctx, int cmd, USUAL *inst, USUAL *item, char *sym, int indexcnt = 0, USUAL *indexlst = 0)
{
  HRESULT r = S_OK;
  if (PSTACK_LEFT) {
    ctx->Ctx.StackTop = &(ctx->Stack[ctx->SP]);
    ctx->Ctx.StackSize = PSTACK_LEFT;
  } else {
    ctx->Ctx.StackTop = 0;
    ctx->Ctx.StackSize = 0;
  } 
  if (ctx->VarAccess) {
    r = ctx->VarAccess(&(ctx->Ctx), cmd, inst, item, sym, indexcnt, indexlst);
    if (r == E_USEDEFAULT)
      r = StdVarAccess(&(ctx->Ctx), cmd, inst, item, sym, indexcnt, indexlst);
  } else {
    r = StdVarAccess(&(ctx->Ctx), cmd, inst, item, sym, indexcnt, indexlst);
  }
  if (r) {
    if (ctx->Ctx.cErrorMsg[0] == 0) {
      sprintf(ctx->Ctx.cErrorMsg, "Unknown error %d (0x%08x) while accessing var '%s'", r, r, sym);
    }
  }
  return r;
}

static HRESULT CallFuncMeth(INTERNALEXECCONTEXT *ctx, USUAL *inst, USUAL *result, char *sym, int parcnt, USUAL *parlst)
{
  HRESULT r = S_OK;
  if (PSTACK_LEFT) {
    ctx->Ctx.StackTop = &(ctx->Stack[ctx->SP]);
    ctx->Ctx.StackSize = PSTACK_LEFT;
  } else {
    ctx->Ctx.StackTop = 0;
    ctx->Ctx.StackSize = 0;
  } 
  if (ctx->FuncCall) {
    r = ctx->FuncCall(&(ctx->Ctx), inst, result, sym, parcnt, parlst);
    if (r == E_USEDEFAULT)
      r = StdFuncCall(&(ctx->Ctx), inst, result, sym, parcnt, parlst);
  } else {
    r = StdFuncCall(&(ctx->Ctx), inst, result, sym, parcnt, parlst);
  }
  if (r) {
    if (ctx->Ctx.cErrorMsg[0] == 0) {
      sprintf(ctx->Ctx.cErrorMsg, "Unknown error %d (0x%08x) while calling '%s'", r, r, sym);
    }
  }
  return r;
}

// Exec the script...
HRESULT WINAPI mdsExecute(HWND hwnd, DWORD userctx, MDS_HEADER *code, USUAL *globals, 
  ProcessCallBack *cb, ProcessFuncCall *fncall, ProcessVarAccess *varacc,
  ProcessDbgHook *dbghook)
{
  if (code->dSignature != MDSH_SIGN) return E_FAIL;
  INTERNALEXECCONTEXT ctx = {0};
  ctx.Ctx.UserCtx = userctx;
  ctx.Ctx.hParent = hwnd;
  ctx.Code = (BYTE *)code;
  if (code->dSymTblOfs) ctx.Symbols = &(ctx.Code[code->dSymTblOfs]);
  ctx.PC = code->dCodeOfs;
  ctx.SP = MAX_EXECSTACK;
  ctx.Debug = false;
  ctx.CallBack = cb;
  ctx.FuncCall = fncall;
  ctx.VarAccess = varacc;
  if (globals) {
    if (globals->ulTag == MAKE_TAG(UT_OBJECT)) {
      if (globals->Value.obj) {
        globals->Value.obj->AddRef();
        ctx.Ctx.Globals = (CUsualRecord *)globals->Value.obj;
      }
    }
  }
  if ((code->dDebugTblLen) && (code->dDebugTblOfs) && (code->dSrcListOfs)) {
    ctx.Debug = true;
    ctx.DbgHook = dbghook;
    ctx.DbgInfoCnt = code->dDebugTblLen;
    ctx.DebugInfo = (DEBUGINFOITEM *)(&(ctx.Code[code->dDebugTblOfs]));
    ctx.SrcList = &(ctx.Code[code->dSrcListOfs]);
  }
  HRESULT r = S_OK;
  if (ctx.Debug) {
    if (ctx.DbgHook) {
      r = ctx.DbgHook(&ctx.Ctx, DBGE_INIT, 0);
      if (r) {
        if (ctx.Ctx.Globals) ctx.Ctx.Globals->Release();
        return r;
      }
    }
  }
  r = E_FAIL;
  __try
  {
  __try
  {
    HRESULT xr = S_OK;
    bool ok = true;
    int cnt = 0;
    srand((unsigned)time(0));
    while (ok) {
      DWORD ofs = ctx.PC;
      
      unsigned short opcode = *((unsigned short *)(&ctx.Code[ctx.PC]));
      ctx.PC += 2;
      if (opcode == 0) break;
      
      if (cb) {
        if (cnt == 0) cb(CB_REFRESH, ctx.Ctx.UserCtx, 0, (DWORD)ctx.Ctx.hParent);
        ++cnt;
        if (cnt == 0xf000) cnt = 0;
      }
      
      // Find and assign debug info
      if (ctx.Debug) {
        if (STACK_LEFT) {
          ctx.Ctx.StackTop = &(ctx.Stack[ctx.SP]);
          ctx.Ctx.StackSize = STACK_LEFT;
        } else {
          ctx.Ctx.StackTop = 0;
          ctx.Ctx.StackSize = 0;
        }
        DEBUGINFOITEM *di = FindDebugInfo(ctx.DebugInfo, ctx.DbgInfoCnt, ofs);
        if (di) {
          if (di->PC != ctx.LastDbgInfo.PC) {
             ctx.LastDbgInfo = *di;
             ctx.Ctx.Line = di->Line;
             if (ctx.SrcList)
               ctx.Ctx.Source = (char *)(&ctx.SrcList[di->SrcNameOffset]);
             if (ctx.DbgHook) xr = ctx.DbgHook(&ctx.Ctx, DBGE_NEWSRCLINE, ofs);
          }   
        }
        if ((xr == S_OK) && (ctx.DbgHook)) xr = ctx.DbgHook(&ctx.Ctx, DBGE_NEWOPCODE, ofs);
        if (xr) break;  
      }
      
      switch (opcode) {
        case OPC_PUSHCN:
          if (ctx.SP) {
            --ctx.SP;          
            UClear(&(ctx.Stack[ctx.SP]));
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
        case OPC_PUSHCI:
          if (ctx.SP) {
            long l = *((long *)(&ctx.Code[ctx.PC]));
            ctx.PC += 4;
            --ctx.SP;
            USetInt(&(ctx.Stack[ctx.SP]), l);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
        case OPC_PUSHCF:
          if (ctx.SP) {
            double f = *((double *)(&ctx.Code[ctx.PC]));
            ctx.PC += 8;
            --ctx.SP;
            USetFloat(&(ctx.Stack[ctx.SP]), f);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
        case OPC_PUSHCC:
          if (ctx.SP) {
            unsigned short len = *((unsigned short *)(&ctx.Code[ctx.PC]));
            ctx.PC += 2;
            char *s = (char *)(&ctx.Code[ctx.PC]);
            ctx.PC += len;
            --ctx.SP;
            USetStrA(&(ctx.Stack[ctx.SP]), s, (int)len);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
        case OPC_PUSHCL:
          if (ctx.SP) {
            short l = *((short *)(&ctx.Code[ctx.PC]));
            ctx.PC += 2;
            --ctx.SP;
            USetLogic(&(ctx.Stack[ctx.SP]), (l != 0));
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
        case OPC_PUSHCS:
          if (ctx.SP) {
            DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
            char *s = (char *)(&(ctx.Symbols[xofs]));
            ctx.PC += 4;
            --ctx.SP;
            UClear(&(ctx.Stack[ctx.SP]));
            ctx.Stack[ctx.SP].ulTag = MAKE_TAG(UT_SYMBOL);
            ctx.Stack[ctx.SP].Value.sym = s;
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;

        case OPC_PUSHV:
          if (ctx.SP) {
            DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
            char *s = (char *)(&(ctx.Symbols[xofs]));
            ctx.PC += 4;
            --ctx.SP;
            UClear(&(ctx.Stack[ctx.SP]));
            xr = CallVarAccess(&ctx, ITEM_GET, 0, &(ctx.Stack[ctx.SP]), s);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack overflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
       
        case OPC_POPV:
          if (STACK_LEFT > 0) {
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;

         case OPC_ASSIGN:
          if (STACK_LEFT > 0) {
            DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
            char *s = (char *)(&(ctx.Symbols[xofs]));
            ctx.PC += 4;
            xr = CallVarAccess(&ctx, ITEM_SET, 0, &(ctx.Stack[ctx.SP]), s);
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
     
        case OPC_ADD:
          if (STACK_LEFT > 1) {
            xr = UAdd(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation + failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_SUB:
          if (STACK_LEFT > 1) {
            xr = USub(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation - failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_MUL:
          if (STACK_LEFT > 1) {
            xr = UMul(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation * failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_DIV:
          if (STACK_LEFT > 1) {
            xr = UDiv(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation / failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_UADD:
          if (STACK_LEFT) {
            if ((TAG_TYPE(ctx.Stack[ctx.SP].ulTag) != UT_INT) && (TAG_TYPE(ctx.Stack[ctx.SP].ulTag) != UT_DOUBLE)) {
               xr = E_INVALIDARG;
               sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation unary + failed, address 0x%08x", ofs);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_USUB:
          if (STACK_LEFT) {
            xr = UNeg(0, &(ctx.Stack[ctx.SP]));
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation unary - failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
     
        case OPC_ARR_GET:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT >= cnt) {
            USUAL u = {0};
            xr = CallVarAccess(&ctx, ITEM_GET, 0, &u, s, cnt, &(ctx.Stack[ctx.SP]));
            if (xr == S_OK) {
              while (cnt > 1) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
              UClear(&(ctx.Stack[ctx.SP]));
              ctx.Stack[ctx.SP] = u;
            } else {
              UClear(&u);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;

        case OPC_ARR_SET:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT > cnt) {
            xr = CallVarAccess(&ctx, ITEM_SET, 0, &(ctx.Stack[ctx.SP]), s, cnt, &(ctx.Stack[ctx.SP+1]));
            if (xr == S_OK) {
              UClear(&(ctx.Stack[ctx.SP]));
              ++ctx.SP;
              while (cnt > 0) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;

        case OPC_ARR_MEMBER_GET:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT > cnt) {
            USUAL u = {0};
            xr = CallVarAccess(&ctx, ITEM_GET, &(ctx.Stack[ctx.SP+cnt]), &u, s, cnt, &(ctx.Stack[ctx.SP]));
            if (xr == S_OK) {
              while (cnt > 1) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
              UClear(&(ctx.Stack[ctx.SP])); // Remove instance ref
              ++ctx.SP;
              UClear(&(ctx.Stack[ctx.SP]));
              ctx.Stack[ctx.SP] = u;
            } else {
              UClear(&u);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;

       
        case OPC_ARR_MEMBER_SET:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT > (cnt+1)) {
            xr = CallVarAccess(&ctx, ITEM_SET, &(ctx.Stack[ctx.SP+cnt+1]), &(ctx.Stack[ctx.SP]), s, cnt, &(ctx.Stack[ctx.SP+1]));
            if (xr == S_OK) {
              UClear(&(ctx.Stack[ctx.SP]));
              ++ctx.SP;
              while (cnt > 0) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
              UClear(&(ctx.Stack[ctx.SP]));
              ++ctx.SP;
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;

     case OPC_MEMBER_GET:
          {
          if (STACK_LEFT > 1) {
            if (ctx.Stack[ctx.SP].ulTag == MAKE_TAG(UT_SYMBOL)) {
              USUAL u = {0};
              char *s = ctx.Stack[ctx.SP].Value.sym;
              xr = CallVarAccess(&ctx, ITEM_GET, &(ctx.Stack[ctx.SP+1]), &u, s);
              if (xr == S_OK) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                UClear(&(ctx.Stack[ctx.SP]));
                ctx.Stack[ctx.SP] = u;
              } else {
                UClear(&u);
              }
            } else {
              sprintf(ctx.Ctx.cErrorMsg, "ERROR: Incorrect member name entry, address 0x%08x", ofs);
              xr = E_FAIL;
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;
       
        case OPC_MEMBER_SET:
          {
          if (STACK_LEFT > 2) {
            if (ctx.Stack[ctx.SP+1].ulTag == MAKE_TAG(UT_SYMBOL)) {
              char *s = ctx.Stack[ctx.SP+1].Value.sym;
              xr = CallVarAccess(&ctx, ITEM_SET, &(ctx.Stack[ctx.SP+2]), &(ctx.Stack[ctx.SP]), s);
              if (xr == S_OK) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
              }
            } else {
              sprintf(ctx.Ctx.cErrorMsg, "ERROR: Incorrect member name entry, address 0x%08x", ofs);
              xr = E_FAIL;
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;
       

        case OPC_CALLS:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT >= cnt) {
            USUAL u = {0};
            UClear(&u);
            if (cnt == 0)
              xr = CallFuncMeth(&ctx, 0, &u, s, 0, 0); else
              xr = CallFuncMeth(&ctx, 0, &u, s, cnt, &(ctx.Stack[ctx.SP]));
            if (xr == S_OK) {
              while (cnt > 0) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
              --ctx.SP;
              ctx.Stack[ctx.SP] = u;
            } else {
              UClear(&u);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;
        
       
        case OPC_METHOD:
          {
          unsigned short cnt = *((unsigned short *)(&ctx.Code[ctx.PC]));
          ctx.PC += 2;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (STACK_LEFT > cnt) {
            USUAL u = {0};
            UClear(&u);
            if (cnt == 0)
              xr = CallFuncMeth(&ctx, &(ctx.Stack[ctx.SP]), &u, s, 0, 0); else
              xr = CallFuncMeth(&ctx, &(ctx.Stack[ctx.SP+cnt]), &u, s, cnt, &(ctx.Stack[ctx.SP]));
            if (xr == S_OK) {
              while (cnt > 0) {
                UClear(&(ctx.Stack[ctx.SP]));
                ++ctx.SP;
                --cnt;
              }
              UClear(&(ctx.Stack[ctx.SP]));
              ctx.Stack[ctx.SP] = u;
            } else {
              UClear(&u);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          }
          break;

        case OPC_NOT:
          if (STACK_LEFT) {
            xr = UNot(0, &(ctx.Stack[ctx.SP]));
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation unary NOT failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_AND:
          if (STACK_LEFT > 1) {
            xr = UAnd(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation AND failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_OR:
          if (STACK_LEFT > 1) {
            xr = UOr(0, &(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]));
            UClear(&(ctx.Stack[ctx.SP]));
            ++ctx.SP;
            if (xr) sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation OR failed, address 0x%08x", ofs);
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
       

        case OPC_EQ:
        case OPC_LESS:
        case OPC_GREAT:
        case OPC_LE:
        case OPC_GE:
        case OPC_NE:
          if (STACK_LEFT > 1) {
            int i = 0;
            xr = UCompare(&(ctx.Stack[ctx.SP+1]), &(ctx.Stack[ctx.SP]), &i);
            if (xr) {
              sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation Compare failed, address 0x%08x", ofs);
            } else {
              UClear(&(ctx.Stack[ctx.SP]));
              ++ctx.SP;
              bool b = false;
              switch (opcode) {
                case OPC_EQ:
                   b = (i == 0);
                   break;
                case OPC_LESS:
                   b = (i < 0);
                   break;
                case OPC_GREAT:
                   b = (i > 0);
                   break;
                case OPC_LE:
                   b = (i <= 0);
                   break;
                case OPC_GE:
                   b = (i >= 0);
                   break;
                case OPC_NE:
                   b = (i != 0);
                   break;
              }
              USetLogic(&(ctx.Stack[ctx.SP]), b);
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
       
        case OPC_JUMP:
          {
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC = xofs;
          }
          break;
          
        case OPC_JT:
        case OPC_JF:
          if (STACK_LEFT) {
            DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
            ctx.PC += 4;
            bool b = false;
            xr = UIsTrue(&(ctx.Stack[ctx.SP]), &b);
            if (xr) {
              sprintf(ctx.Ctx.cErrorMsg, "ERROR: Operation IsTrue failed, address 0x%08x", ofs);
            } else {
              UClear(&(ctx.Stack[ctx.SP]));
              ++ctx.SP;
              if (opcode == OPC_JF) b = !b;
              if (b) ctx.PC = xofs;
            }
          } else {
            sprintf(ctx.Ctx.cErrorMsg, "ERROR: Stack underflow, address 0x%08x", ofs);
            xr = E_FAIL;
          }
          break;
          
        case OPC_QUIT:
          ok = false;
          break;
          
        case OPC_NOP:
          // Do nothing
          break;
          
        case OPC_BREAK:
          xr = E_BREAK;
          sprintf(ctx.Ctx.cErrorMsg, "BREAK signaled");
          break;
          
        case OPC_STDOUT:
          {
          int xlen = *((int *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          DWORD xofs = *((DWORD *)(&ctx.Code[ctx.PC]));
          ctx.PC += 4;
          char *s = (char *)(&(ctx.Symbols[xofs]));
          if (xlen) {
             USUAL u = {0};
             xr = USetStrA(&u, s, xlen);
             if (xr == S_OK) {
               USUAL res = {0};
               UClear(&res);
               xr = CallFuncMeth(&ctx, 0, &res, "__stdout", 1, &u);
               UClear(&res);
               UClear(&u);
             } else {
               sprintf(ctx.Ctx.cErrorMsg, "Error processing STDOUT");
             }
          }
          }
          break;
    
        default:
          sprintf(ctx.Ctx.cErrorMsg, "ERROR: Invalid opcode %d [%04x], address 0x%08x", opcode, opcode, ofs);
          xr = E_FAIL;
      }
      if (xr) ok = false;
    }
    r = xr;
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    r = E_FAIL;
    DWORD exc = GetExceptionCode();
    switch (exc) {
    case EXCEPTION_ACCESS_VIOLATION:
      strcpy(ctx.Ctx.cErrorMsg, "FATAL: Access violation");
      break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      strcpy(ctx.Ctx.cErrorMsg, "FATAL: Bound checking error");
      break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      strcpy(ctx.Ctx.cErrorMsg, "FATAL: Divide by zero");
      break;
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_INT_OVERFLOW:
      strcpy(ctx.Ctx.cErrorMsg, "FATAL: Numeric overflow");
      break;
    default:
      sprintf(ctx.Ctx.cErrorMsg, "FATAL: Exception 0x%08x occured", exc);
    }
    
  }
  }
  __finally
  {
    if (r) {
      char msg[1024];
      if (ctx.Ctx.cErrorMsg[0] == 0)
        sprintf(ctx.Ctx.cErrorMsg, "Error %d (0x%08x)", r, r);
      if ((ctx.LastDbgInfo.Line) && (ctx.SrcList)) {
        char *s = (char *)(&ctx.SrcList[ctx.LastDbgInfo.SrcNameOffset]);
        sprintf(msg, "%s, Line %d in %s", ctx.Ctx.cErrorMsg, ctx.LastDbgInfo.Line, s);
        strcpy(ctx.Ctx.cErrorMsg, msg);
      }
      if (ctx.CallBack) {
        if (r == E_BREAK) {
          ctx.CallBack(CB_MESSAGE, ctx.Ctx.UserCtx, ctx.Ctx.cErrorMsg, (DWORD)ctx.Ctx.hParent); 
        } else {
          ctx.CallBack(CB_ERROR, ctx.Ctx.UserCtx, ctx.Ctx.cErrorMsg, (DWORD)ctx.Ctx.hParent);
        }
      }
      if (ctx.Debug && ctx.DbgHook && (r != E_BREAK)) ctx.DbgHook(&ctx.Ctx, DBGE_ERROR, (DWORD)ctx.Ctx.cErrorMsg);
      // if (cb && (r != E_BREAK)) cb(CB_ERROR, ctx.Ctx.UserCtx, ctx.Ctx.cErrorMsg);
    }
    if (ctx.Debug && ctx.DbgHook) ctx.DbgHook(&ctx.Ctx, DBGE_CLOSE, r);
    while (ctx.SP < MAX_EXECSTACK) {
      UClear(&(ctx.Stack[ctx.SP]));
      ++ctx.SP;
    }
    if (ctx.Ctx.Globals) ctx.Ctx.Globals->Release();
    if (ctx.Ctx.Locals) ctx.Ctx.Locals->Release();
  }
  return r;
}

#pragma pack(push, 1)
typedef struct _LEXEMITEM
{
  unsigned short Next;
  short What;
  unsigned short SymLen;
} LEXEMITEM, *PLEXEMITEM;
#pragma pack(pop)

bool mdsDisassemble(char *lstname, BYTE *code)
{
  if (code == 0) return false;
  MDS_HEADER *h = (MDS_HEADER *)code;
  DWORD pc = h->dCodeOfs;
  BYTE *symbols = 0;
  DEBUGINFOITEM *dbginfo = 0;
  BYTE *srclist = 0;
  if (h->dSymTblOfs) symbols = &(code[h->dSymTblOfs]);
  if (h->dDebugTblOfs) dbginfo = (DEBUGINFOITEM *)(&(code[h->dDebugTblOfs])); 
  if ((dbginfo) && (h->dSrcListOfs)) srclist = &(code[h->dSrcListOfs]);
  HANDLE hout = CreateFile(
        lstname,
        GENERIC_WRITE,                // open for writing 
        0,                            // do not share 
        NULL,                         // default security 
        CREATE_ALWAYS,                // overwrite existing 
        FILE_ATTRIBUTE_NORMAL,         // asynchronous I/O 
        NULL);
  if (hout == INVALID_HANDLE_VALUE) return false;
  char buf[2048];
  bool ok = true;
  while (ok) {
    DWORD ofs = pc;
    
    if ((dbginfo) && (srclist)) {
      DEBUGINFOITEM *di = FindDebugInfo(dbginfo, h->dDebugTblLen, ofs);
      if (di) {
        char *s = (char *)(&srclist[di->SrcNameOffset]);
        sprintf(buf, "Line %d, file %s\r\n", di->Line, s);
        DWORD read = 0;
        WriteFile(hout, buf, (DWORD)strlen(buf), &read, NULL);
      }
    }
    DWORD xofs = 0;
    unsigned short cnt = 0;
    char *s = 0;
    unsigned short opcode = *((unsigned short *)(&code[pc]));
    pc += 2;
    buf[0] = 0;
    if (opcode == 0) break;
    switch (opcode) {
     case OPC_STDOUT:
       {
       DWORD xlen = *((DWORD *)(&code[pc]));
       pc += 4;
       xofs = *((DWORD *)(&code[pc]));
       pc += 4;
       s = (char *)(&(symbols[xofs]));
       char b[255];
       if (xlen > 128) xlen = 128;
       memcpy(b, s, xlen);
       b[xlen] = 0;
       sprintf(buf, "%08x: STDOUT '%s'\r\n", ofs, b);
       }
       break;
       
     case OPC_PUSHCN:
       sprintf(buf, "%08x: PUSH <NIL>\r\n", ofs);
       break;
     case OPC_PUSHCI:
       {
       long l = *((long *)(&code[pc]));
       pc += 4;
       sprintf(buf, "%08x: PUSH INT %d (0x%08x)\r\n", ofs, l, l);
       }
       break;
     case OPC_PUSHCF:
       {
       double f = *((double *)(&code[pc]));
       pc += 8;
       sprintf(buf, "%08x: PUSH FLOAT %f\r\n", ofs, f);
       }
       break;
     case OPC_PUSHCC:
       {
       short len = *((short *)(&code[pc]));
       pc += 2;
       s = (char *)(&(code[pc]));
       pc += len;
       if (len > 254) len = 254;
       char b[255];
       memcpy(b, s, len);
       b[len] = 0;
       sprintf(buf, "%08x: PUSH STR (%d) '%s'\r\n", ofs, len, b);
       }
       break;
     case OPC_PUSHCL:
       {
       short l = *((short *)(&code[pc]));
       pc += 2;
       sprintf(buf, "%08x: PUSH LOGIC %d\r\n", ofs, l);
       }
       break;
     case OPC_PUSHCS:
       {
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: PUSH SYMBOL <%s>\r\n", ofs, s);
       }
       break;

     case OPC_PUSHV:
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: PUSH VAR <%s>\r\n", ofs, s);
       break;
       
     case OPC_POPV:
       sprintf(buf, "%08x: POP\r\n", ofs);
       break;

     case OPC_ASSIGN:
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: ASSIGN TO <%s>\r\n", ofs, s);
       break;
     
     case OPC_ADD:
       sprintf(buf, "%08x: ADD +\r\n", ofs);
       break;
     case OPC_SUB:
       sprintf(buf, "%08x: SUB -\r\n", ofs);
       break;
     case OPC_MUL:
       sprintf(buf, "%08x: MUL *\r\n", ofs);
       break;
     case OPC_DIV:
       sprintf(buf, "%08x: DIV /\r\n", ofs);
       break;
     case OPC_UADD:
       sprintf(buf, "%08x: UADD +\r\n", ofs);
       break;
     case OPC_USUB:
       sprintf(buf, "%08x: USUB -\r\n", ofs);
       break;
     
     case OPC_ARR_GET:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: ARRAY GET <%s> (%d)\r\n", ofs, s, cnt);
       break;
       
     case OPC_ARR_SET:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: ARRAY SET <%s> (%d)\r\n", ofs, s, cnt);
       break;
       
     case OPC_ARR_MEMBER_GET:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: ARRAY MEMBER GET <%s> (%d)\r\n", ofs, s, cnt);
       break;
       
     case OPC_ARR_MEMBER_SET:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: ARRAY MEMBER SET <%s> (%d)\r\n", ofs, s, cnt);
       break;
     
     case OPC_MEMBER_GET:
       sprintf(buf, "%08x: MEMBER GET\r\n", ofs);
       break;
       
     case OPC_MEMBER_SET:
       sprintf(buf, "%08x: MEMBER SET\r\n", ofs);
       break;

     case OPC_NOT:
       sprintf(buf, "%08x: NOT !\r\n", ofs);
       break;
     case OPC_AND:
       sprintf(buf, "%08x: AND\r\n", ofs);
       break;
     case OPC_OR:
       sprintf(buf, "%08x: OR\r\n", ofs);
       break;
     case OPC_EQ:
       sprintf(buf, "%08x: EQ ==\r\n", ofs);
       break;
     case OPC_LESS:
       sprintf(buf, "%08x: LESS <\r\n", ofs);
       break;
     case OPC_GREAT:
       sprintf(buf, "%08x: GREAT >\r\n", ofs);
       break;
     case OPC_LE:
       sprintf(buf, "%08x: LE <=\r\n", ofs);
       break;
     case OPC_GE:
       sprintf(buf, "%08x: GE >=\r\n", ofs);
       break;
     case OPC_NE:
       sprintf(buf, "%08x: NE !=\r\n", ofs);
       break;

     case OPC_JUMP:
       xofs = *((DWORD *)(&code[pc]));
       pc += 4;
       sprintf(buf, "%08x: JUMP %08x\r\n", ofs, xofs);
       break;
     case OPC_JT:
       xofs = *((DWORD *)(&code[pc]));
       pc += 4;
       sprintf(buf, "%08x: JT %08x\r\n", ofs, xofs);
       break;
     case OPC_JF:
       xofs = *((DWORD *)(&code[pc]));
       pc += 4;
       sprintf(buf, "%08x: JF %08x\r\n", ofs, xofs);
       break;
     case OPC_CALLS:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: CALLS (%d) <%s>\r\n", ofs, cnt, s);
       break;
     case OPC_METHOD:
       cnt = *((unsigned short *)(&code[pc]));
       pc += 2;
       xofs = *((DWORD *)(&code[pc]));
       s = (char *)(&(symbols[xofs]));
       pc += 4;
       sprintf(buf, "%08x: METHOD (%d) %s\r\n", ofs, cnt, s);
       break;
     case OPC_QUIT:
       sprintf(buf, "%08x: QUIT\r\n", ofs);
       break;
     case OPC_NOP:
       sprintf(buf, "%08x: NOP\r\n", ofs);
       break;
     case OPC_BREAK:
       sprintf(buf, "%08x: BREAK\r\n", ofs);
       break;
    
    default:
      sprintf(buf, "ERROR: Invalid opcode %d [%04x], address 0x%08x\r\n", opcode, opcode, ofs);
      ok = false;
    }
    if (buf[0] != 0) {
      DWORD read = 0;
      if (!WriteFile(hout, buf, (DWORD)strlen(buf), &read, NULL)) break;
    }
  }
  CloseHandle(hout);
  return true;
}

static bool StartWith(char *str, char *word)
{
  int l = (int)strlen(word);
  if (l > 127) return false;
  if ((int)strlen(str) < l) return false;
  char buf[128];
  memcpy(buf, str, l);
  buf[l] = 0;
  return (_stricmp(buf, word) == 0);
}

static void AllTrim(char *dest, char *str)
{
  int l = (int)strlen(str);
  int p = 0;
  while (p < l) {
    if (str[p] != ' ') break;
    ++p;
  }
  strcpy(dest, &(str[p]));
  l = (int)strlen(dest);
  p = (l - 1);
  while (p >= 0) {
   if (dest[p] != ' ') break;
   --p;
  }
  dest[p+1] = 0;
}

static void GetCmdParam(char *dest, int ofs, char *str)
{
  char b[512];
  AllTrim(b, &(str[ofs]));  
  int l = (int)strlen(b);
  if (l == 0) {
    dest[0] = 0;
    return;
  }
  if ((b[0] == '"') && (b[l-1] == '"')) {
    b[l-1] = 0;
    AllTrim(dest, &(b[1]));  
  } else {
    strcpy(dest, b);
  }
}

static void SplitPath(char *file, char *dir, char *path)
{
  int l = (int)strlen(path);
  if (l == 0) {
    if (file) file[0] = 0;
    if (dir) dir[0] = 0;
    return;
  }
  int i = l - 1;
  while (i >= 0) {
    if (path[i] == '\\') break;
    i--;
  }
  if (file) strcpy(file, &(path[i+1]));
  if (dir) {
    if (i >= 0) {
      memcpy(dir, path, i+1);
      dir[i+1] = 0;
    } else {
      dir[0] = 0;
    }
  }
}


// Statics
static int At2(char *search, char *str)
{
  int l = (int)strlen(str);
  int sl = (int)strlen(search);
  if (sl > l) return -1;
  int i = 0;
  while (i <= (l - sl)) {
    bool match = true;
    for (int j = 0; j < sl; j++) {
      if (str[i + j] != search[j]) {
        match = false;
        break;
      }
    }  
    if (match) return i;
    i++;
  }
  return -1;
}

static void SubStr3(char *dest, char *str, int start, int len)
{
  int p = 0;
  int i = (start - 1);
  int l = (int)strlen(str);
  while (i < l) {
    if (p == len) break;
    dest[p] = str[i];
    ++p;
    ++i;
  }
  dest[p] = 0;
}

static int FmtDate(FASTDATE *d, char *buf)
{
  UNPACKEDDATE ud = {0};
  buf[0] = 0;
  if (fdUnpackDate(d, &ud)) return 0;
  return wsprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", 
    ud.Year, ud.Month, ud.Day, ud.Hour, ud.Min, ud.Sec);
}


static void ReplaceExt(char *fname, char *ext)
{
  int l = (int)strlen(fname);
  if (l == 0) return;
  while (l > 0) {
    if (fname[l-1] == '.') {
      fname[l] = 0;
      strcat(fname, ext);
      return;
    }
    if (fname[l-1] == '\\') return;
    l--;
  }
}

static bool SymCmp(char *sym, char *str)
{
  for (int i = 0; i < MAX_SYMBOL; i++) {
    if (sym[i] != str[i]) return false;
    if (sym[i] == 0) break;
  }
  return true; 
}

static int CharAt(char ch, char *str)
{
  int i = 0;
  while (str[i] != 0) {
    if (ch == str[i]) return i;
    ++i;  
  }
  return -1;
}

static void mdsAddLex(BYTE *cBuffer, int *nL, int nWhat, char *cS)
{
  int nSL;
  nSL = (int)strlen(cS);
  char sym[MAX_LINE+1];
  char *p;
  //
  if ((nSL > 0) && (nWhat == XT_SYMBOL)) {
     if (CharAt(cS[0], "0123456789\0") >= 0) {
        nWhat = XT_NUMBER;
     }
  }
  //
  if (nWhat != XT_STRING) {
    AllTrim(sym, cS);
    _strlwr(sym);
    // _strupr(sym);
    p = sym;
    nSL = (int)strlen(p);
  } else {
    p = cS;
  }
  // Смотрим, а не операторы ли это...
  if ((nWhat == XT_SYMBOL)&&
      ((strcmp(p, "and") == 0)||(strcmp(p, "not") == 0)||(strcmp(p, "or") == 0))) {
     nWhat = XT_OPERATOR;
  }
  //
  int l = *nL;
  //
  LEXEMITEM *li = (LEXEMITEM *)(&(cBuffer[l]));
  li->Next = (unsigned short)(l + sizeof(LEXEMITEM) + nSL + 1);
  li->What = (short)nWhat;
  li->SymLen = (unsigned short)nSL;
  l += sizeof(LEXEMITEM);
  if (nSL) {
    memcpy(&(cBuffer[l]), p, nSL);
    l += nSL;
    cBuffer[l] = 0;
    ++l;
  }
  *nL = l;
  //
}

static bool mdsIsNum(char *s)
{
  int i = 0;
  while (s[i]) {
    if (CharAt(s[i], "0123456789\0") < 0) return false;
    ++i;
  }
  return true;
}

static int mdsParseExpr(char *cExpr, BYTE *cBuf)
//******************************************************************
//
// Разделяет формулу по лексемам.
// Возвращает 0, если все в порядке, либо код ошибки
//
//******************************************************************
{
char cC;
char cS[MAX_LINE+1];
int nP; int nWhat;
int nL; char nSC;
int sl; int cl;
   //
   nP = 0;
   nWhat = XT_NONE;
   cS[0] = 0;
   cl = 0;
   nL = 0;
   nSC = 0;
   sl = (int)strlen(cExpr);
   //
   while (nP < sl) {
      //
      cC = cExpr[nP];
      //
      if (nWhat == XT_STRING) { // Вводим строку
         if (nSC == cC) {
            mdsAddLex(cBuf, &nL, XT_STRING, cS);
            nWhat = XT_NONE;
            cS[0] = 0;
            cl = 0;
         } else {
            cS[cl] = cC;
            ++cl;
            cS[cl] = 0;
         }
      } else {
         if (CharAt(cC, "\"'\0") >= 0) { // Начало строки
           if (nWhat != XT_NONE) {
              mdsAddLex(cBuf, &nL, nWhat, cS);
              cS[0] = 0;
              cl = 0;
              nWhat = XT_NONE;
           }
           nSC = cC;
           nWhat = XT_STRING;
         //
         } else if (CharAt(cC, DELIM_SET) >= 0) { // Разделитель...
           if (nWhat != XT_NONE) {
              mdsAddLex(cBuf, &nL, nWhat, cS);
              cS[0] = 0;
              cl = 0;
              nWhat = XT_NONE;
           }
         //
         } else if (CharAt(cC, UNC_SET) >= 0) { //
           if (nWhat != XT_NONE) {
              mdsAddLex(cBuf, &nL, nWhat, cS);
              cS[0] = 0;
              cl = 0;
              nWhat = XT_NONE;
           }
           //
           char c[2];
           c[0] = cC;
           c[1] = 0;
           mdsAddLex(cBuf, &nL, XT_OPERATOR, c);
         //
         } else if (CharAt(cC, MNC_SET) >= 0) {
           if ((nWhat != XT_NONE)&&(nWhat != XT_OPERATOR)) {
              mdsAddLex(cBuf, &nL, nWhat, cS);
              cS[0] = 0;
              cl = 0;
              nWhat = XT_NONE;
           }
           //
           nWhat = XT_OPERATOR;
           cS[cl] = cC;
           ++cl;
           cS[cl] = 0;
           //
         } else if (CharAt(cC, ERROR_SET) >= 0) {
           return 1;
         } else {
           //
           bool dot = false;
           //
           if (cC == '.') {
             dot = true;
             if ((nWhat == XT_SYMBOL) && (cl) && mdsIsNum(cS)) dot = false;
           }
           //
           if ((nWhat != XT_SYMBOL) || dot) {
              if (nWhat != XT_NONE) {
                mdsAddLex(cBuf, &nL, nWhat, cS);
                cS[0] = 0;
                cl = 0;
                nWhat = XT_NONE;
              }
           }
           //
           if (dot) {
             char c[2];
             c[0] = cC;
             c[1] = 0;
             mdsAddLex(cBuf, &nL, XT_OPERATOR, c);
           } else {
             nWhat = XT_SYMBOL;
             cS[cl] = cC;
             ++cl;
             cS[cl] = 0;
           }
           //
         }
      }
      //
      nP++;
      //
   }
   //
   if (nWhat == XT_STRING) {
      return 1;
   }
   if (nWhat != XT_NONE) {
      mdsAddLex(cBuf, &nL, nWhat, cS);
   }
   // Put last 0 item...
   LEXEMITEM *li = (LEXEMITEM *)(&(cBuf[nL]));
   memset(li, 0, sizeof(LEXEMITEM));
   return 0;
}


// CMDCompiler
CMDCompiler::CMDCompiler()
{

  acVarStack = (VARSTACKITEM *)mmAlloc(MAX_STACK * sizeof(VARSTACKITEM));
  memset(acVarStack, 0, MAX_STACK * sizeof(VARSTACKITEM));
  acOpStack = (OPSTACKITEM *)mmAlloc(MAX_STACK * sizeof(OPSTACKITEM));
  memset(acOpStack, 0, MAX_STACK * sizeof(OPSTACKITEM));
  acCStack = (CSTACKITEM *)mmAlloc(MAX_CSTACK * sizeof(CSTACKITEM));
  memset(acCStack, 0, MAX_CSTACK * sizeof(CSTACKITEM));
  
  memset(&pcCodeBuf, 0, sizeof(BUFFERITEM));
  memset(&pcSymBuf, 0, sizeof(BUFFERITEM));
  memset(&pcSrcList, 0, sizeof(BUFFERITEM));
  
  acSourceLst = (SOURCEITEM *)mmAlloc(MAX_SOURCE * sizeof(SOURCEITEM));
  memset(acSourceLst, 0, MAX_SOURCE * sizeof(SOURCEITEM));
  ncSource = -1;
  
  lcClear = true;
  ncCSP = 0;
  ncOSP = 0;
  ncVSP = 0;
  ncLType = 0;
  ncStrInfo = 0;
  ncLineTotal = 0;
  dwBufLen = 0;
  pCode = 0;
  ncData = 0;
  ncLPC = -1;
  memset(&ucLItem, 0, sizeof(USUAL));
  lList = false;
  hWnd = 0;
  pDebugInfo = 0;
  hcLstFile = 0;
  ccLLst[0] = 0;
  ncError = 0;
  icALastOp = 0;
  icAParam = 0;
  ccASymbol[0] = 0;
  pCallBack = 0;
  dwCBCtx = 0;
}


CMDCompiler::~CMDCompiler()
{
  dwBufLen = 0;
  if (pCode) mmFree(pCode);
  pCode = 0;
  BufAlloc(&pcCodeBuf, 0);
  BufAlloc(&pcSymBuf, 0);
  BufAlloc(&pcSrcList, 0);
  if (pDebugInfo) mmFree(pDebugInfo);
  for (int i = 0; i < MAX_STACK; i++) {
    if (acVarStack[i].Str) mmFree(acVarStack[i].Str);
    acVarStack[i].Str = 0;
  }
  mmFree(acVarStack);
  mmFree(acOpStack);
  mmFree(acCStack);
  for (int i = 0; i < MAX_SOURCE; i++) {
    if (acSourceLst[i].Reader) delete acSourceLst[i].Reader;
    acSourceLst[i].Reader = 0;
  }
  mmFree(acSourceLst);
  ncSource = -1;
  UClear(&ucLItem);
  if (hcLstFile) {
    CloseHandle(hcLstFile);
    hcLstFile = 0;
  }
}

bool CMDCompiler::BufAdd(BUFFERITEM *buf, void *data, DWORD datalen)
{
  DWORD s = buf->dwPos + datalen;
  if (s >= MAX_BUFFER) {
    RaiseError(100, "Buffer memory limit exceeded");
    return false;
  }
  if (s >= buf->dwSize) { // Too small...
    while (s >= buf->dwSize) {
      buf->dwSize += (buf->dwSize >> 1);
    }
    if (buf->dwSize > MAX_BUFFER) buf->dwSize = MAX_BUFFER;
    void *p = mmAlloc(buf->dwSize);
    if (p == 0) {
      RaiseError(101, "Buffer allocating error");
      return false;
    }
    memset(p, 0, buf->dwSize);
    memcpy(p, buf->pBuf, buf->dwPos);
    mmFree(buf->pBuf);
    buf->pBuf = (BYTE *)p;
  }
  memcpy(&(buf->pBuf[buf->dwPos]), data, datalen);
  buf->dwPos += datalen;
  
  return true;
}

bool CMDCompiler::BufAlloc(BUFFERITEM *buf, DWORD size)
{
  if (buf->pBuf) mmFree(buf->pBuf);
  buf->pBuf = 0;
  buf->dwSize = 0;
  buf->dwPos = 0;
  if (size == 0) return true;
  buf->pBuf = (BYTE *)mmAlloc(size);
  if (buf->pBuf == 0) return false;
  buf->dwSize = size;
  memset(buf->pBuf, 0, buf->dwSize);
  return true;
}

void CMDCompiler::RaiseError(int nC, char *cT)
{
  char buf[1024];
  sprintf(buf, "Error #%d: %s", nC, cT);
  WriteLst(buf);
  StdErrMsg(nC, cT);
  ncError = nC;
  throw(MDS_EXC_SIGNATURE);
}

bool CMDCompiler::WriteLst(char *cS)
{
  char cX[1024];
  if (ccLLst[0] != 0) {
     strcpy(cX, ccLLst);
     ccLLst[0] = 0;
     WriteLst(cX);
  }
  if ((hcLstFile == 0) || (cS[0] == 0)) return true;
  strcpy(cX, cS);
  strcat(cX, "\r\n");
  DWORD read = 0;
  return (WriteFile(hcLstFile, cX, (DWORD)strlen(cX), &read, NULL) == TRUE);
}

bool CMDCompiler::StdErrMsg(int nCode, char *cText)
{
  char buf[1024];
  if ((ncSource >= 0) && (acSourceLst[ncSource].Reader)) {
    sprintf(buf, "Error #%d\rLine: %d\rFile: %s\r\r%s", nCode, 
      acSourceLst[ncSource].Reader->Line(),
      acSourceLst[ncSource].FileName,
      cText);
  } else {
    sprintf(buf, "Error #%d\r\r%s", nCode, cText);
  }
  if (pCallBack) {
    pCallBack(CB_ERROR, dwCBCtx, buf, (DWORD)hWnd);
  } else {
    MessageBox(0, buf, "", MB_OK | MB_TASKMODAL | MB_ICONERROR);
  }
  return false;
}

void CMDCompiler::PlaceOp(unsigned short dOp)
{
  BufAdd(&pcCodeBuf, &dOp, 2);
  ncLPC = -1;
  icALastOp = 0;
}

void CMDCompiler::PlaceOffset(DWORD dwOffset)
{
  BufAdd(&pcCodeBuf, &dwOffset, 4);
}

void CMDCompiler::OpStatus(char *cOp, int *nCode, int *nPrior)
{
//***************************************************************************
// Обработка оператора, cOp - строка с содержимым оператора
// Возвращает @nCode - код оператора, @nPrior - его приоритет
//***************************************************************************
   *nCode = 0;
   *nPrior = 0;
   // Математичекие
    if (SymCmp(cOp, "+")) {
          *nCode = OP_PLUS;
          *nPrior = 100;
    } else if (SymCmp(cOp, "-")) {
          *nCode = OP_MINUS;
          *nPrior = 100;
    } else if (SymCmp(cOp, "*")) {
          *nCode = OP_MUL;
          *nPrior = 110;
    } else if (SymCmp(cOp, "/")) {
          *nCode = OP_DIV;
          *nPrior = 110;
    } else if (SymCmp(cOp, "=")) {
          *nCode = OP_ASSIGN;
          *nPrior = 1;
      // Логические
    } else if (SymCmp(cOp, "and")) {
          *nCode = OP_AND;
          *nPrior = 50;
    } else if (SymCmp(cOp, "or")) {
          *nCode = OP_OR;
          *nPrior = 50;
    } else if (SymCmp(cOp, "not") || SymCmp(cOp, "!")) {
          *nCode = OP_NOT;
          *nPrior = 20;
    } else if (SymCmp(cOp, "==")) {
          *nCode = OP_LEQ;
          *nPrior = 50;
    } else if (SymCmp(cOp, "<")) {
          *nCode = OP_LESS;
          *nPrior = 50;
    } else if (SymCmp(cOp, ">")) {
          *nCode = OP_GREAT;
          *nPrior = 50;
    } else if (SymCmp(cOp, ">=")) {
          *nCode = OP_GE;
          *nPrior = 50;
    } else if (SymCmp(cOp, "<=")) {
          *nCode = OP_LE;
          *nPrior = 50;
     } else if (SymCmp(cOp, "<>") || SymCmp(cOp, "><") || SymCmp(cOp, "!=")) {
          *nCode = OP_NE;
          *nPrior = 50;
      // Остальные
      } else if (SymCmp(cOp, "[")) {
          *nCode = OP_LSPAR;
          *nPrior = 2;
      } else if (SymCmp(cOp, "]")) {
          *nCode = OP_RSPAR;
          *nPrior = 2;
      } else if (SymCmp(cOp, "(")) {
          *nCode = OP_LPAR;
          *nPrior = 2;
      } else if (SymCmp(cOp, ")")) {
          *nCode = OP_RPAR;
          *nPrior = 2;
      } else if (SymCmp(cOp, ",")) {
          *nCode = OP_COMMA;
          *nPrior = 2;
      } else if (SymCmp(cOp, ":") || SymCmp(cOp, ".")) {
          *nCode = OP_MEMBER;
          *nPrior = 120;
      } else {
          char buf[255];
          sprintf(buf, "Unknown operator '%s'", cOp);
          RaiseError(1005, buf);
      }
}

void CMDCompiler::PutOp(int nCode, int nPriority, char *cOp)
{
//******************************************************************
//
//  Ложит на верхушку стека оператор
//
//******************************************************************
int nSP; char buf[512];

   nSP = ncOSP;
   //
   if (nCode == OP_COMMA) { // Запятая
      if ((ncLType == 2) && ((ucLItem.Value.l == OP_COMMA)||
          (ucLItem.Value.l == OP_LPAR))) {
         PutConst("N", NULL_STRING); // Ложим константу...
      } else {
         DropTo(3); // Сброс операторов...
      }
   } else if (nCode == OP_RPAR) { // Закрывающая ) скобка
      //
      if ((ncLType == 2) && (ucLItem.Value.l == OP_COMMA)) {
         PutConst("N", NULL_STRING); // Ложим константу...
      }
      DropTo(2);
      
   } else if (nCode == OP_RSPAR) { // Закрывающая ] скобка
      //
      if ((ncLType == 2) && (ucLItem.Value.l == OP_COMMA)) {
         RaiseError(1037, "Array index must not be empty");
      }
      DropTo(2);

   } else if (nCode == OP_LPAR) {
      //
      nSP++;
      if (nSP >= MAX_STACK) RaiseError(1018, "Operator stack overflow");
      //
      if (ncLType == 1) { // Последний - символ, значит - функция...
         //
         if (ncLPC < 0) RaiseError(1019, "Invalid function name");
         
         acOpStack[nSP].Priority = nPriority; // {nPriority, OP_LFPAR, ncVSP} // Заносим указатель на последнюю запись
                                                               // стека переменных
         acOpStack[nSP].Code = OP_LFPAR;
         acOpStack[nSP].Ref = ncVSP;
         ccLLst[0] = 0;
         pcCodeBuf.dwPos = ncLPC;
         ncLPC = -1;
         //
      } else {
         //
         acOpStack[nSP].Code = nCode; // {nPriority, nCode}
         acOpStack[nSP].Priority = nPriority;
         acOpStack[nSP].Ref = 0;
         //
      }
      //
      ncOSP = nSP;
      //
   } else if (nCode == OP_LSPAR) {
      //
      nSP++;
      if (nSP >= MAX_STACK) RaiseError(1018, "Operator stack overflow");
      //
      if (ncLType == 1) { // Последний - символ, значит - массив...
         //
         if (ncLPC < 0) RaiseError(1019, "Invalid array name");
         
         acOpStack[nSP].Priority = nPriority; // {nPriority, OP_LFPAR, ncVSP} // Заносим указатель на последнюю запись
                                                               // стека переменных
         acOpStack[nSP].Code = OP_LSPAR;
         acOpStack[nSP].Ref = ncVSP;
         ccLLst[0] = 0;
         pcCodeBuf.dwPos = ncLPC;
         ncLPC = -1;
         //
      } else {
         RaiseError(1036, "Array name is not specified");
      }
      //
      ncOSP = nSP;
      //
   } else {
      //
      if ((ncLType == 2)&& // Последним был оператор
          (ucLItem.Value.l != OP_RPAR) &&
          (ucLItem.Value.l != OP_RSPAR)) { // И не скобка
         if (ucLItem.Value.l > 1000) { // Это был унарный оператор
            RaiseError(1020, "Operator using mismatch");
         } else if (nCode == OP_PLUS) {
            nCode = OP_UPLUS;
            nPriority = 60; // Низкий приоритет
         } else if (nCode == OP_MINUS) {
            nCode = OP_UMIN;
            nPriority = 60; // Низкий приоритет
         } else if (nCode == OP_NOT) {
            nCode = OP_NOT;
         } else if (nCode == OP_MEMBER) {
            RaiseError(1054, "Incorrect usage of member operator");
         } else {
            sprintf(buf, "Operator '%s' cannot be unary", cOp);
            RaiseError(1021, buf);
         }
      }
      //
      //
      if (nSP > 0) {
         if (acOpStack[nSP].Priority >= nPriority) DropTo(nPriority);
      }
      if (nCode == OP_ASSIGN) {
        /*
        if (ncVSP > 0) {
          for (int i = 1; i <= ncVSP; i++) {
            sprintf(buf, ">>>> VAR[%d] Type:%d Name:'%s' Op:%d Par:%d", i, acVarStack[i].Type, acVarStack[i].Name, 
            acVarStack[i].iOpCode, acVarStack[i].iParam);
            WriteLst(buf);
          }
        }
        */
      
        if ((icALastOp == 0) || (ncVSP != 1)) RaiseError(1055, "Incorrect usage of assign operator");
        
        // Copy data to assign item
        strcpy(acVarStack[ncVSP].Name, ccASymbol);
        acVarStack[ncVSP].iOpCode = icALastOp;
        acVarStack[ncVSP].iParam = icAParam;
        // Rolling back put variable to stack (or last operation) for assignment
        if (icALastOp == OPC_PUSHV) {
          pcCodeBuf.dwPos -= 6;
          ccLLst[0] = 0;
        } else if (icALastOp == OPC_MEMBER_GET) {
          pcCodeBuf.dwPos -= 2;
          ccLLst[0] = 0;
        } else if (icALastOp == OPC_ARR_MEMBER_GET) {
          pcCodeBuf.dwPos -= 8;
          ccLLst[0] = 0;
        } else if (icALastOp == OPC_ARR_GET) {
          pcCodeBuf.dwPos -= 8;
          ccLLst[0] = 0;
        } else {
          RaiseError(1056, "Error processing assign operator");
        }
        /*
        sprintf(buf, ">>>> Type:%d Name:'%s' Op:%d Par:%d", acVarStack[ncVSP].Type, acVarStack[ncVSP].Name, 
          acVarStack[ncVSP].iOpCode, acVarStack[ncVSP].iParam);
        WriteLst(buf);
        */
        sprintf(buf, "        [%8x] =", pcCodeBuf.dwPos);
        WriteLst(buf);
      }
      //
      nSP = ncOSP;
      nSP++;
      if (nSP >= MAX_STACK) RaiseError(1023, "Operator stack overflow");
      //
      acOpStack[nSP].Priority = nPriority; // {nPriority, nCode}
      acOpStack[nSP].Code = nCode;
      acOpStack[nSP].Ref = 0;
      ncOSP = nSP;
      //
   }
   //
   ncLType = 2;
   USetInt(&ucLItem, nCode);
}

void CMDCompiler::DropTo(int nPrior)
{
//********************************************************************
// Сброс операторов до оператора с большим
// или равным приоритетом...
//********************************************************************
int nSP;
OPSTACKITEM *aO;
int nC; char cM[255];
int nPC; DWORD nKop; int nVSP; int nL; int nX; int nCnt;
DWORD nO;
char buf[1024];
   //
   nSP = ncOSP;
   //
   if (nSP == 0) {
      // RaiseError(302, "Operator stack underflow")
      return;
   }
   //
   while (true) {
      //
      aO = &(acOpStack[nSP]);
      //
      if (aO->Priority >= nPrior) {
       //
       nC = aO->Code; // Код оператора
       //
       if ((nPrior == 2)&&((nC == OP_LPAR)||(nC == OP_LFPAR))) { // Скобка
          //
          bool member = false;
          if (nC == OP_LFPAR) { // Открывающая функцию скобка...
             //
             nX = acOpStack[nSP].Ref; // Адрес функции в стеке
             //
             nCnt = 0; // Количество переменных
             //
             while (ncVSP > nX) { // Выкидываем из стека параметры функции
                DropVar();
                nCnt++;
             }
             //
             strcpy(cM, acVarStack[ncVSP].Name);
             nL = (int)strlen(cM);
             
             if (nSP > 1) {
               if (acOpStack[nSP-1].Code == OP_MEMBER) {
                 member = true;
               }
             }
             //
             nPC = pcCodeBuf.dwPos;
             if (member)
               PlaceOp(OPC_METHOD); else 
               PlaceOp(OPC_CALLS);
             PlaceOp((unsigned short)nCnt);
             nO = PlaceSymbol(cM, nL);
             //
             if (lList) {
                _strupr(cM);
                if (member) 
                  sprintf(buf, "        [%8x] METHOD <%d> %s [%x]", nPC, nCnt, cM, nO); else
                  sprintf(buf, "        [%8x] CALLS <%d> %s [%x]", nPC, nCnt, cM, nO);
                WriteLst(buf);
             }
             //
             DropVar();
             // Как бы результат функции...
             if (!member) {
               ++ncVSP;
               acVarStack[ncVSP].Type = 1; // {1, "N", "NIL"}
               strcpy(acVarStack[ncVSP].VType, "N");
               strcpy(acVarStack[ncVSP].Name, "NIL");
             }
             //
          }
          //
          memset(&(acOpStack[nSP]), 0, sizeof(OPSTACKITEM)); // Выкидываем из стека
          nSP--;
          if (member) {
             memset(&(acOpStack[nSP]), 0, sizeof(OPSTACKITEM)); // Выкидываем из стека
             nSP--;
          }
          break;
       }
       if ((nPrior == 2)&&(nC == OP_LSPAR)) { // Скобка [ (массив)
          //
          nX = acOpStack[nSP].Ref; // Адрес функции в стеке
          //
          nCnt = 0; // Количество переменных
          //
          while (ncVSP > nX) { // Выкидываем из стека параметры функции
             DropVar();
             nCnt++;
          }
          //
          strcpy(cM, acVarStack[ncVSP].Name);
          nL = (int)strlen(cM);
          //
          bool member = false;
          if (nSP > 1) {
            if (acOpStack[nSP-1].Code == OP_MEMBER) {
              member = true;
            }
          }
          nPC = pcCodeBuf.dwPos;
          if (member) {
            PlaceOp(OPC_ARR_MEMBER_GET);
            PlaceOp((unsigned short)nCnt);
            nO = PlaceSymbol(cM, nL);
            icALastOp = OPC_ARR_MEMBER_GET;
            icAParam = nCnt;
            strcpy(ccASymbol, cM);
            //
            if (lList) {
              if (strlen(ccLLst) > 0) WriteLst("");
              sprintf(ccLLst, "        [%8x] ARR_MEMBER_GET '%s'(%d) [%x]", nPC, cM, nCnt, nO);
            }
          } else {
            PlaceOp(OPC_ARR_GET);
            PlaceOp((unsigned short)nCnt);
            nO = PlaceSymbol(cM, nL);
            icALastOp = OPC_ARR_GET;
            icAParam = nCnt;
            strcpy(ccASymbol, cM);
            //
            if (lList) {
              if (strlen(ccLLst) > 0) WriteLst("");
              _strupr(cM);
              sprintf(ccLLst, "        [%8x] ARR_GET %s(%d) [%x]", nPC, cM, nCnt, nO);
            }
          }
          //
          DropVar();
          // Как бы результат функции...
          if (!member) {
            ++ncVSP;
            acVarStack[ncVSP].Type = 0; // {1, "N", "NIL"}
            strcpy(acVarStack[ncVSP].VType, "");
            strcpy(acVarStack[ncVSP].Name, cM);
          }
          //
          memset(&(acOpStack[nSP]), 0, sizeof(OPSTACKITEM)); // Выкидываем из стека
          nSP--;
          if (member) {
             memset(&(acOpStack[nSP]), 0, sizeof(OPSTACKITEM)); // Выкидываем из стека
             nSP--;
          }
          break;
       }
       nPC = pcCodeBuf.dwPos;
       if (!((nC == OP_LPAR)||(nC == OP_RPAR)||(nC == OP_COMMA))) {
            // Арифметичекие
            if (nC == OP_MINUS) {
               DropVar();
               strcpy(cM, "OP -");
               nKop = OPC_SUB;
            } else if (nC == OP_PLUS) {
               DropVar();
               strcpy(cM, "OP +");
               nKop = OPC_ADD;
            } else if (nC == OP_MUL) {
               DropVar();
               strcpy(cM, "OP *");
               nKop = OPC_MUL;
            } else if (nC == OP_DIV) {
               DropVar();
               strcpy(cM, "OP /");
               nKop = OPC_DIV;
            } else if (nC == OP_UMIN) {
               strcpy(cM, "OP UNARY -");
               nKop = OPC_USUB;
            } else if (nC == OP_UPLUS) {
               strcpy(cM, "OP UNARY +");
               nKop = OPC_UADD;
            // Логические
            } else if (nC == OP_AND) {
               DropVar();
               strcpy(cM, "OP L AND");
               nKop = OPC_AND;
            } else if (nC == OP_OR) {
               DropVar();
               strcpy(cM, "OP L OR");
               nKop = OPC_OR;
            } else if (nC == OP_LEQ) {
               DropVar();
               strcpy(cM, "OP L ==");
               nKop = OPC_EQ;
            } else if (nC == OP_LESS) {
               DropVar();
               strcpy(cM, "OP L <");
               nKop = OPC_LESS;
            } else if (nC == OP_GREAT) {
               DropVar();
               strcpy(cM, "OP L >");
               nKop = OPC_GREAT;
            } else if (nC == OP_LE) {
               DropVar();
               strcpy(cM, "OP L <=");
               nKop = OPC_LE;
            } else if (nC == OP_GE) {
               DropVar();
               strcpy(cM, "OP L >=");
               nKop = OPC_GE;
            } else if (nC == OP_NE) {
               DropVar();
               strcpy(cM, "OP L <>");
               nKop = OPC_NE;
            } else if (nC == OP_NOT) {
               strcpy(cM, "OP L NOT (UNARY)");
               nKop = OPC_NOT;
            } else if (nC == OP_MEMBER) {
               DropVar();
               strcpy(cM, "MEMBER_GET");
               nKop = OPC_MEMBER_GET;
            // Присвоения
            } else if (nC == OP_ASSIGN) {
               DropVar();
               nVSP = ncVSP;
               //
               if (acVarStack[nVSP].Type != 0) { // Это константа
                  RaiseError(1024, "Assign to constant");
               }
               //
               if (nVSP != 1) RaiseError(1025, "Error in expression");
               //
               if (acVarStack[nVSP].iOpCode == OPC_PUSHV) {
               
                 PlaceOp(OPC_ASSIGN);
                 nL = (int)strlen(acVarStack[nVSP].Name);
                 nO = PlaceSymbol(acVarStack[nVSP].Name, nL);
                 if (lList) {
                    char b[128];
                    strcpy(b, acVarStack[nVSP].Name);
                    _strupr(b);
                    sprintf(cM, "ASSIGN '%s' [%x]", b, nO);
                 }
                 
               } else if (acVarStack[nVSP].iOpCode == OPC_ARR_GET) {
               
                  PlaceOp(OPC_ARR_SET);
                  PlaceOp((unsigned short)acVarStack[nVSP].iParam);
                  int nL = (int)strlen(acVarStack[nVSP].Name);
                  int nO = PlaceSymbol(acVarStack[nVSP].Name, nL);
                  if (lList) {
                    sprintf(cM, "ARR_SET '%s'(%d) [%x]", acVarStack[nVSP].Name, acVarStack[nVSP].iParam, nO);
                  }
                  
               } else if (acVarStack[nVSP].iOpCode == OPC_MEMBER_GET) {
                 
                  PlaceOp(OPC_MEMBER_SET);
                  if (lList) {
                    sprintf(cM, "MEMBER_SET");
                  }
                 
               } else if (acVarStack[nVSP].iOpCode == OPC_ARR_MEMBER_GET) {
               
                 PlaceOp(OPC_ARR_MEMBER_SET);
                 PlaceOp((unsigned short)acVarStack[nVSP].iParam);
                 int nL = (int)strlen(acVarStack[nVSP].Name);
                 int nO = PlaceSymbol(acVarStack[nVSP].Name, nL);
                 if (lList) {
                   sprintf(cM, "ARR_MEMBER_SET '%s'(%d) [%x]", acVarStack[nVSP].Name, acVarStack[nVSP].iParam, nO);
                 }
                 
               } else {
                  RaiseError(1056, "Error processing assign operator");
               }
               lStackClear = true;
               
         }
         //
         if (lList) {
            if (nKop == OPC_MEMBER_GET) {
              if (strlen(ccLLst) > 0) WriteLst("");
              sprintf(ccLLst, "        [%8x] %s", nPC, cM);
            } else {
              sprintf(buf, "        [%8x] %s", nPC, cM);
              WriteLst(buf);
            }
         }
         //
         if (nC != OP_ASSIGN) PlaceOp((unsigned short)nKop);
         if (nKop = OPC_MEMBER_GET) {
            icALastOp = OPC_MEMBER_GET;
            icAParam = 0;
            ccASymbol[0] = 0;
         }
         //
       }
       //
      } else {
        break;
      }
      //
      memset(&(acOpStack[nSP]), 0, sizeof(OPSTACKITEM));
      //
      nSP--;
      ncOSP = nSP;
      //
      if (nSP == 0) break;
   }
   //
   ncOSP = nSP;
}

void CMDCompiler::DropVar()
{
//*******************************************************************
//
// Сброс переменной из стека
//
//*******************************************************************
int nSP;
   //
   nSP = ncVSP;
   //
   if (nSP == 0) {
      RaiseError(1026, "Variable stack underflow");
   }
   if (acVarStack[nSP].Str) mmFree(acVarStack[nSP].Str);
   memset(&(acVarStack[nSP]), 0, sizeof(VARSTACKITEM));
   nSP--;
   ncVSP = nSP;
   //
}

DWORD CMDCompiler::AddBufSymbol(BUFFERITEM *buf, char *sym)
{
	char *pB = (char *)(buf->pBuf);
	DWORD nOfs = 0;
	bool found = false;
	
	if (pB[nOfs] != 0) {
	 while (true) {
	   char *s = &(pB[nOfs]);
	   if (strcmp(s, sym) == 0) {
	     found = true;
	     break;  
	   }
	   while (pB[nOfs] != 0) ++nOfs;
	   ++nOfs;
	   if (pB[nOfs] == 0) break;
	 }
	}
	
	if (!found) {
	  if (buf->dwPos) --buf->dwPos; // Roll back last 0 char
	  nOfs = buf->dwPos;
	  BufAdd(buf, sym, (DWORD)strlen(sym));
	  WORD w = 0;
	  BufAdd(buf, &w, 2); // Add 2 zero bytes
	}
	
	return nOfs;
}

DWORD CMDCompiler::PlaceSymbol(char *sOp, int nL)
{
  DWORD nOfs; 
  char sym[128];
  memcpy(sym, sOp, nL);
  sym[nL] = 0;
  nOfs = AddBufSymbol(&pcSymBuf, sym);
  BufAdd(&pcCodeBuf, &nOfs, 4);
  ncLPC = -1;
  
  return nOfs;

}

void CMDCompiler::PutSymbol(char *cSymbol)
{
//******************************************************************
//
//  Ложит на верхушку стека символ
//
//******************************************************************
int nSP; int nPC; int nL; int nX; DWORD nO;
char buf[512];
   //
   nSP = ncVSP;
   nPC = pcCodeBuf.dwPos;
   nSP++;
   //
   if (nSP >= MAX_STACK) {
      RaiseError(1017, "Variable stack overflow");
   }
   // Проверка на стандартные символы
   if ((strcmp(cSymbol, "true") == 0)||(strcmp(cSymbol, "false") == 0)) {
      //
      acVarStack[nSP].Type = 1; // {1, "L", cSymbol}
      strcpy(acVarStack[nSP].VType, "L");
      strcpy(acVarStack[nSP].Name, cSymbol);
      ncVSP = nSP;
      //
      if (lList) {
         sprintf(buf, "        [%8x] PUSHCL %s", nPC, cSymbol);
         WriteLst(buf);
      }
      //
      PlaceOp(OPC_PUSHCL);
      //
      if (strcmp(cSymbol, "true") == 0) {
         PlaceOp(1);
      } else {
         PlaceOp(0);
      }
      //
      ncData += 1; // Длина данных
      //
      ncLType = 1;
      USetStrA(&ucLItem, cSymbol);
      //
      return;
      //
   }
   //
   bool member = false;
   if (ncOSP > 0) {
      if (acOpStack[ncOSP].Code == OP_MEMBER) {
        member = true;
      }
   }
   acVarStack[nSP].Type = 0; // {0, "", cSymbol}
   strcpy(acVarStack[nSP].VType, "");
   strcpy(acVarStack[nSP].Name, cSymbol);
   ncVSP = nSP;
   //
   ncLType = 1;
   USetStrA(&ucLItem, cSymbol);
   //
   if (lList && (strlen(ccLLst) > 0)) {
      WriteLst("");
   }
   //
   nL = (int)strlen(cSymbol);
   nX = pcCodeBuf.dwPos;
   //
   if (member) {
     PlaceOp(OPC_PUSHCS);
     nO = PlaceSymbol(cSymbol, nL);
     if (lList) {
        sprintf(ccLLst, "        [%8x] PUSHCS '%s' [%x]", nPC, cSymbol, nO);
     }
   } else {
     PlaceOp(OPC_PUSHV);
     nO = PlaceSymbol(cSymbol, nL);
     icALastOp = OPC_PUSHV;
     icAParam = 0;
     strcpy(ccASymbol, cSymbol);
     if (lList) {
        char b[255];
        strcpy(b, cSymbol);
        _strupr(b);
        sprintf(ccLLst, "        [%8x] PUSHS %s [%x]", nPC, b, nO);
     }
   }
   //
   ncData += nL; // Длина данных
   //
   ncLPC = nX;
}

void CMDCompiler::PutConst(char *cType, BYTE *cContent)
{
//******************************************************************
//
//  Ложит на верхушку стека константу
//
//******************************************************************
int nSP; char cS[255]; int nPC; int nL; DWORD nKop;
char buf[512];
   //
   nSP = ncVSP;
   nPC = pcCodeBuf.dwPos;
   nSP++;
   //
   if (nSP >= MAX_STACK) {
      RaiseError(1016, "Variable stack overflow");
   }
   //
   acVarStack[nSP].Type = 1; // {1, cType, cContent}
   strcpy(acVarStack[nSP].VType, cType);
   if (cType[0] == 'C') {
     int l = 0;
     if (cContent) l = (int)strlen((char *)cContent);
     acVarStack[nSP].Str = (char *)mmAlloc(l + 1);
     if (acVarStack[nSP].Str == 0) return;
     if (cContent) memcpy(acVarStack[nSP].Str, cContent, l);
     acVarStack[nSP].Str[l] = 0;
   } else {
     acVarStack[nSP].Name[0] = 0;
     if (cContent) strcpy(acVarStack[nSP].Name, (char *)cContent);
   }
   ncVSP = nSP;
   //
   ncLType = 1;
   if (cContent) 
     USetStrA(&ucLItem, (char *)cContent); else
     USetStrA(&ucLItem, "");
   //
   //
   if (hcLstFile) {
      if (cType[0] == 'N') {
         strcpy(cS, "(NIL)");
      } else if (cType[0] == 'I') {
         sprintf(cS, "%d", *((int *)cContent));
      } else if (cType[0] == 'F') {
         sprintf(cS,  "%f", *((double *)cContent));
      } else if (cType[0] == 'C') {
         int l = (int)strlen((char *)cContent);
         if (l > 80) l = 80;
         memcpy(cS, cContent, l);
         cS[l] = 0;
      }
      if (lList) {
         if (cType[0] == 'C') {
           sprintf(buf, "        [%8x] PUSHC%s '%s'", nPC, cType, cS);
         } else {
           sprintf(buf, "        [%8x] PUSHC%s %s", nPC, cType, cS);
         }
         WriteLst(buf);
      }
   }
   //
   nL = 0;
   if (cType[0] == 'N') {
      nL = 0;
      nKop = OPC_PUSHCN;
   } else if (cType[0] == 'I') {
      nL = sizeof(long);
      nKop = OPC_PUSHCI;
   } else if (cType[0] == 'F') {
      nL = sizeof(double);
      nKop = OPC_PUSHCF;
   } else if (cType[0] == 'C') {
      nL = (int)strlen((char *)cContent);
      nKop = OPC_PUSHCC;
   }
   //
   PlaceOp((unsigned short)nKop);
   //
   if (cType[0] == 'C') PlaceOp((unsigned short)nL);
   //
   if (nL > 0) {
      BufAdd(&pcCodeBuf, cContent, nL);
      ncLPC = -1;
   }
   //
   ncData += nL; // Длина данных
   //
}

void CMDCompiler::Lex(int nType, char *cLex, int nLex, int *cCommand)
{
//***********************************************************************
// Обработка лексемы
// nType - тип лексемы, cLex - сама лексема...
// nLex - номер лексемы
//***********************************************************************
int nCode; int nPrior; char cType[MAX_SYMBOL+1];
int ni; int nN; int nSP;
// aI AS ARRAY, 
int nA; int nT; char cLP[128];
char buf[1024];
  //
  *cCommand = CMD_NONE;
  //
  if ((nLex == 1) && (lcClear)) {
    //
    lcClear = false;
    //
    if (nType == XT_SYMBOL) { // Смотрим, а не зарезервированное ли слово...
      if (SymCmp(cLex, "while")) {
          //
          nSP = ncCSP;
          nSP++;
          //
          if (nSP >= MAX_CSTACK) RaiseError(1006, "Internal stack overflow");
          //
          if (lList) {
            sprintf(buf, "        [%8x] WH %04dS:", pcCodeBuf.dwPos, nSP);
            WriteLst(buf);
          }
          //
          memset(&acCStack[nSP], 0, sizeof(CSTACKITEM));
          acCStack[nSP].Type = 1; // {1, {pcCodeBuf.dwPos, 0, 0}, {}}
          acCStack[nSP].Start[0] = pcCodeBuf.dwPos;
          acCStack[nSP].Start[1] = 0;
          acCStack[nSP].Start[2] = 0;
          acCStack[nSP].FixupCnt = 0;
          
          ncCSP = nSP;
          //
          ncStrInfo = DO_WHILE;
          //
          return;
          //
        // ПРЕКРАТИТЬ...
        }
      else if (SymCmp(cLex, "break")) {
          //
          nSP = ncCSP;
          while ((nSP > 0) && (acCStack[nSP].Type != 1)) {
             nSP--;
          }
          //
          if (nSP == 0) RaiseError(1007, "Outside WHILE loop");
          //
          if (lList) {
             sprintf(buf, "        [%8x] JUMP WH %04dE", pcCodeBuf.dwPos, nSP);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_JUMP);
          acCStack[nSP].FixupList[acCStack[nSP].FixupCnt].PC = pcCodeBuf.dwPos;
          acCStack[nSP].FixupList[acCStack[nSP].FixupCnt].Type = 2;
          ++acCStack[nSP].FixupCnt;
          PlaceOffset(0);
          //
          *cCommand = CMD_BREAK;
          return;
          //
          }
        // СНАЧАЛА...
        else if (SymCmp(cLex, "loop")) {
          //
          nSP = ncCSP;
          while ((nSP > 0) && (acCStack[nSP].Type != 1)) {
             nSP--;
          }
          //
          if (nSP == 0) RaiseError(1008, "Outside WHILE loop");
          //
          if (lList) {
             sprintf(buf, "        [%8x] JUMP WH %04dS", pcCodeBuf.dwPos, nSP);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_JUMP);
          PlaceOffset((DWORD)acCStack[nSP].Start[0]);
          //
          *cCommand = CMD_BREAK;
          return;
          //
          }
        else if (SymCmp(cLex, "quit")) {
          //
          if (lList) {
             sprintf(buf, "        [%8x] QUIT", pcCodeBuf.dwPos);
             WriteLst(buf);
          }
          PlaceOp(OPC_QUIT);
          *cCommand = CMD_BREAK;
          return;
          //
          }
        else if (SymCmp(cLex, "stop")) {
          //
          if (lList) {
             sprintf(buf, "        [%8x] STOP", pcCodeBuf.dwPos);
             WriteLst(buf);
          }
          PlaceOp(OPC_BREAK);
          *cCommand = CMD_BREAK;
          return;
          }
        // ЕСЛИ ...
        else if (SymCmp(cLex, "if")) {
          //
          nSP = ncCSP;
          nSP++;
          //
          if (nSP >= MAX_CSTACK) RaiseError(1009, "Internal stack overflow");
          //
          memset(&acCStack[nSP], 0, sizeof(CSTACKITEM));
          acCStack[nSP].Type = 0; // {0, {pcCodeBuf.dwPos, 0, 0}, {}}
          acCStack[nSP].Start[0] = pcCodeBuf.dwPos;
          acCStack[nSP].Start[1] = 0;
          acCStack[nSP].Start[2] = 0;
          acCStack[nSP].FixupCnt = 0;
          
          ncCSP = nSP;
          //
          ncStrInfo = DO_IF;
          //
          return;
          }
        // КОНЕЦ ...
        else if (SymCmp(cLex, "end")) {
          //
          nSP = ncCSP;
          //
          if (nSP == 0) RaiseError(1010, "Outside any control structure");
          //
          CSTACKITEM *aI = &(acCStack[nSP]);
          //
          if (aI->Type == 0) { // Это ЕСЛИ...
             if (aI->Start[2] == 0) { // Не было ИНАЧЕ
                aI->Start[2] = pcCodeBuf.dwPos;
             }
             //
             aI->Start[1] = pcCodeBuf.dwPos;
             strcpy(cLP, "IF");
          } else { // Это ПОКА...
             // Заполняем переход на начало
             if (lList) {
                sprintf(buf, "        [%8x] JUMP WH%04dS", pcCodeBuf.dwPos, nSP);
                WriteLst(buf);
             }
             //
             PlaceOp(OPC_JUMP);
             PlaceOffset((DWORD)aI->Start[0]); // Переход на начало
             aI->Start[1] = pcCodeBuf.dwPos;
             //
             strcpy(cLP, "WH");
          }
          // Связываем ссылки
          if ((nN = aI->FixupCnt) > 0) {
             for (ni = 0; ni < nN; ni++) {
                nA = aI->FixupList[ni].PC;
                nT = aI->Start[aI->FixupList[ni].Type - 1];
                memcpy(&pcCodeBuf.pBuf[nA], &nT, 4);
             }
          }
          //
          if (lList) {
             sprintf(buf, "        [%8x] %s%04dE:", pcCodeBuf.dwPos, cLP, nSP);
             WriteLst(buf);
             sprintf(buf, "        [%8x] NOP", pcCodeBuf.dwPos);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_NOP);
          lStackClear = true;
          //
          memset(&acCStack[nSP], 0, sizeof(CSTACKITEM));
          --ncCSP;
          //
          *cCommand = CMD_BREAK;
          return;
          //
          }
        // ИНАЧЕ ...
        else if (SymCmp(cLex, "else")) {
          nSP = ncCSP;
          //
          if (nSP == 0) RaiseError(1011, "Outside IF structure");
          //
          if (acCStack[nSP].Type != 0) { // Это не IF
             RaiseError(1012, "Outside IF structure");
          }
          // Переходим на конец цикла...
          if (lList) {
             sprintf(buf, "        [%8x] JUMP IF%04dE", pcCodeBuf.dwPos, nSP);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_JUMP);
          lStackClear = true;
          // Привязка по концу цикла
          acCStack[nSP].FixupList[acCStack[nSP].FixupCnt].PC = pcCodeBuf.dwPos;
          acCStack[nSP].FixupList[acCStack[nSP].FixupCnt].Type = 2;
          ++acCStack[nSP].FixupCnt;
          PlaceOffset(0);
          //
          acCStack[nSP].Start[2] = pcCodeBuf.dwPos;
          //
          if (lList) {
             sprintf(buf, "        [%8x] IF%04dX:", pcCodeBuf.dwPos, nSP);
             WriteLst(buf);
          }
          //
          *cCommand = CMD_BREAK;
          return;
          //
       }
    }
  }
  //
  // Операторы...
  if (nType == XT_OPERATOR) {
         //
         if (SymCmp(cLex, ";")) { // Окончание строки
            *cCommand = CMD_CONT;
            return;
         }
         OpStatus(cLex, &nCode, &nPrior);
         PutOp(nCode, nPrior, cLex);

     // Строка...
     }
   else if (nType == XT_STRING) {
        //
        if (ncLType != 0) {
           if (ncLType == 1) { // Был символ...
              RaiseError(1013, "Operator or delimeter expected");
           }
        }
        //
        PutConst("C", (BYTE *)cLex);
     }
     // Число...
    else if (nType == XT_NUMBER) {
        //
        if (ncLType != 0) {
           if (ncLType == 1) { // Был символ...
              RaiseError(1013, "Operator or delimeter expected");
           }
        }
        //
        ni = 0;
        while (cLex[ni] != 0) {
          if (CharAt(cLex[ni], "0123456789.\0") < 0) {
             sprintf(buf, "Invalid numeric format (%s)", cLex);
             RaiseError(1014, buf);
          }
          ++ni;
        }
        //
        if (CharAt('.', cLex) < 0) { // Целое число
           strcpy(cType, "I");
        } else {
           strcpy(cType, "F");
        }
        //
        if (strcmp(cType, "I") == 0) {
           long l = atol(cLex);
           PutConst(cType, (BYTE *)(&l));
        } else {
           double d = atof(cLex);
           PutConst(cType, (BYTE *)(&d));
        }
        //
     }
     // Символ...
   else if (nType == XT_SYMBOL) {
        //
        if (ncLType != 0) {
           if (ncLType == 1) { // Был символ...
              RaiseError(1013, "Operator or delimeter expected");
           }
        }
        //
        PutSymbol(cLex);
        //
  } else {
      sprintf(buf, "Unknown lexem type '%s'", cLex);
      RaiseError(1015, buf);
  }
}

void CMDCompiler::ViewStatus()
{
  if (pCallBack) {
     char buf[255];
     int nL1;
     int nL2;
     if ((ncSource >= 0) && (acSourceLst[ncSource].Reader)) {
       nL1 = acSourceLst[ncSource].Reader->Line();
       nL2 = ncLineTotal + nL1;
     } else {
       nL1 = ncLineTotal;
       nL2 = nL1;
     }
     sprintf(buf, "Line: %d", nL2);
     pCallBack(CB_INFO, dwCBCtx, buf, (DWORD)hWnd);
     pCallBack(CB_REFRESH, dwCBCtx, 0, (DWORD)hWnd);
  }
}

MDS_HEADER *CMDCompiler::DetachCode()
{
  MDS_HEADER *h = (MDS_HEADER *)pCode;
  pCode = 0;
  return h;
}

void CMDCompiler::NotifyExpr(char *cE)
{
}

void CMDCompiler::OpenSource(char *srcfile)
{
  strcpy(acSourceLst[ncSource].FileName, srcfile);
  acSourceLst[ncSource].Reader = new CSrcReader();
  if (acSourceLst[ncSource].Reader == 0) {
     RaiseError(2, "Out of memory");
  }
  if (acSourceLst[ncSource].Reader->Open(srcfile) != S_OK) {
     char buf[512];
     sprintf(buf, "Error opening source file: %s [%s]", srcfile, acSourceLst[ncSource].Reader->cErrMsg);
     delete acSourceLst[ncSource].Reader;
     acSourceLst[ncSource].Reader = 0;
     RaiseError(1, buf);
  }
}

#define MAX_FULLLINE    8191
#define MAX_PARSEBUFFER 32768

DWORD CMDCompiler::Compile(HWND hwnd, char *filename, DWORD dFlg, ProcessCallBack *cb, DWORD cbctx)
{
// ******************************************************************
// Компилирует содержимое файла cFile, и возвращает
// строку, в которой находится откомпилированный код.
// bErrorHandler - кодоблок, который обрабатывает ошибки (выводит
// сообщения о них), в качестве параметра получает код ошибки и
// строковое сообщение о ней.
// Если была ошибка, или компиляция прервана, то возвращает NIL
// Если указано pDebugInfo - в нем возвращается отладочная
// информация в формате <Номер строки><Счетчик> по 2 байта на рыло.
// ******************************************************************
BYTE *cBuf;
int nType; int nDItem; int nCommand; int nN;
char cLstFile[512];
MDS_HEADER *rH; 
DWORD dRet;
SOURCEITEM *pSrc;
char cS[MAX_FULLLINE+1];
char buf[1024];
int ncLine;
char *cLex;

  //
  nCommand = 0;
  nDItem = 0;
  lList = false;
  hWnd = hwnd;
  pSrc = 0;
  ncLine = 0;
  cBuf = 0;
  dwBufLen = 0;
  if (pCode) mmFree(pCode);
  pCode = 0;
  
  if (cb) {
    pCallBack = cb;
    dwCBCtx = cbctx;
  } else {
    pCallBack = 0;
    dwCBCtx = 0;
  }
  //
  if ((dFlg & MDSF_DEBUGINFO) != 0L) {
     pDebugInfo = (DEBUGINFOITEM *)mmAlloc((MAX_SRCLINE + 2) * sizeof(DEBUGINFOITEM));
     if (pDebugInfo == 0) return RET_FAILED;
     memset(pDebugInfo, 0, (MAX_SRCLINE + 2) * sizeof(DEBUGINFOITEM));
     if (!BufAlloc(&pcSrcList, 2048)) {
       if (pDebugInfo) mmFree(pDebugInfo);
       return RET_FAILED;
     }
  } else {
     pDebugInfo = 0;
  }
  //
  if (!BufAlloc(&pcSymBuf, 8192)) {
    if (pDebugInfo) mmFree(pDebugInfo);
    BufAlloc(&pcSrcList, 0);
    return RET_FAILED;
  }
  if (!BufAlloc(&pcCodeBuf, 8192)) {
    BufAlloc(&pcSymBuf, 0);
    BufAlloc(&pcSrcList, 0);
    if (pDebugInfo) mmFree(pDebugInfo);
    return RET_FAILED;
  }
  pcCodeBuf.dwPos = sizeof(MDS_HEADER); // Initial code offset
  
  cBuf = (BYTE *)mmAlloc(MAX_PARSEBUFFER);
  if (cBuf == 0) {
    if (pDebugInfo) mmFree(pDebugInfo);
    BufAlloc(&pcSymBuf, 0);
    BufAlloc(&pcCodeBuf, 0);
    BufAlloc(&pcSrcList, 0);
    return RET_FAILED;
  }
  memset(cBuf, 0, MAX_PARSEBUFFER);
  
  ncError = 0;
  dRet = 0;
  //
  try 
  {
  //
  ncSource = 0;
  OpenSource(filename);
  //
  ccLLst[0] = 0;
  if ((dFlg & MDSF_LISTING) != 0L) {
  	 //
  	 strcpy(cLstFile, filename);
  	 ReplaceExt(cLstFile, "lst");
	   hcLstFile = CreateFile(
        cLstFile,
        GENERIC_WRITE,                // open for writing 
        0,                            // do not share 
        NULL,                         // default security 
        CREATE_ALWAYS,                // overwrite existing 
        FILE_ATTRIBUTE_NORMAL,         // asynchronous I/O 
        NULL);
     if (hcLstFile == INVALID_HANDLE_VALUE) {
       hcLstFile = 0;
       RaiseError(3, "Error creating list file");
     }
     lList = true;
     sprintf(buf, "Compilation listing: %s", filename);
     WriteLst(buf);
     FASTDATE fd = {0};
     fdCurDate(&fd);
     char dt[128];
     FmtDate(&fd, dt);
     sprintf(buf, "Created %s", dt);
     WriteLst(buf);
     WriteLst("");
  }
  //
  ncLineTotal = 0;
  //
  ViewStatus();
  //
  // Работаем по файлу...
  //
  bool newsrc = false;
  lStackClear = false;
  while (true) { // Until source stack is not empty...
   newsrc = false;
   pSrc = &(acSourceLst[ncSource]); // Current source...
   cS[0] = 0;
   unsigned short sSrcOfs = 0;
   if ((dFlg & MDSF_DEBUGINFO) != 0) {
     char src[_MAX_PATH];
     SplitPath(src, 0, pSrc->FileName);
     sSrcOfs = (unsigned short)AddBufSymbol(&pcSrcList, src);
   }
   
   if (lList) {
     sprintf(buf, "> Source file: '%s'", pSrc->FileName);
     WriteLst(buf);
   }
   
   int iStartLine = 0;
  
   while (!pSrc->Reader->Eof()) {
     //
     if (pcCodeBuf.dwPos >= (MAX_BUFFER - BUFFER_X)) {
        sprintf(buf, "Code size too large (%d)", MAX_BUFFER);
        RaiseError(1000, buf);
     }
     //
     if ((ncLine & 0x0000003F) == 0L) {
        ViewStatus();
     }
     
     int explen = 0;
     int exptype = 0;
     
     if (ncLine >= MAX_SRCLINE) {
     	  RaiseError(5000, "Source file(s) are too big");
     }
     
     char buf[MAX_LINE+1];
     
     if (pSrc->Reader->NextExpr(buf, MAX_LINE, &explen, &exptype) != S_OK) {
        RaiseError(5000, "Error reading next expression");
     }
     ncLine = pSrc->Reader->ExpLine();
     //
     if (exptype == EXPT_TEXT) {
       int nPC = pcCodeBuf.dwPos;
       PlaceOp(OPC_STDOUT);
       PlaceOffset((DWORD)explen);
       if (pcSymBuf.dwPos) --pcSymBuf.dwPos;
       DWORD ofs = pcSymBuf.dwPos;
       BufAdd(&pcSymBuf, (void *)buf, explen);
       WORD w = 0;
       BufAdd(&pcSymBuf, (void *)&w, 2);
       PlaceOffset(ofs);
       if (lList) {
         char b[128];
         if (explen > 127) explen = 127;
         memcpy(b, buf, explen);
         b[explen] = 0;
         sprintf(buf, "        [%8x] STDOUT '%s' [%x]", nPC, b, ofs);
         WriteLst(buf);
       }
       continue;
     }
     if (exptype != EXPT_CODE) {
       RaiseError(5001, "Unknown expression type");
     }
     //
     AllTrim(cS, buf);
     if (strlen(cS) == 0) continue;
     //
     NotifyExpr(cS);
     //
     if (lList) {
        int l = (int)strlen(cS);
        if (l > 127) l = 127;
        char b[128];
        memcpy(b, cS, l);
        b[l] = 0;
        sprintf(buf, "#%6d %s", ncLine, b);
        WriteLst(buf);
     }
     //
     // Process #include statement
     //
     if (StartWith(cS, "include ")) {
       char fn[_MAX_PATH];
       GetCmdParam(fn, 8, cS);
       if (CharAt('\\', fn) < 0) { // Add path...
         char dir[_MAX_PATH];
         SplitPath(0, dir, pSrc->FileName);
         strcat(dir, fn);
         strcpy(fn, dir);
       }
       if (ncSource == MAX_SOURCE) RaiseError(1043, "Too many nested includes");
       ++ncSource;
       OpenSource(fn);
       newsrc = true;
       break; 
     }
     
     //
     if (mdsParseExpr(cS, cBuf) != 0) {
        sprintf(buf, "Syntax error, file %s, line #%d", pSrc->FileName, ncLine);
        RaiseError(1000, buf);
     }
     //
     cS[0] = 0;
     icALastOp = 0;
     icAParam = 0;
     ccASymbol[0] = 0;
     lStackClear = false;
     // *******************************************************************
     // *          В переменной cBuf - разобранная строка                 *
     // *******************************************************************
     // Отладочная информация
     if ((dFlg & MDSF_DEBUGINFO) != 0) {
        pDebugInfo[nDItem].Line = (unsigned short)ncLine;
        pDebugInfo[nDItem].PC = pcCodeBuf.dwPos;
        pDebugInfo[nDItem].SrcNameOffset = sSrcOfs;
        nDItem++;
     }
     //
     LEXEMITEM *li = (LEXEMITEM *)cBuf;
     nN = 0;
     while (li->What) {
     
        nType = li->What;
        cLex = (char *)(((BYTE *)li) + sizeof(LEXEMITEM));
        // **************************************************************
        // *          nType - тип лексемы, cLex - сама лексема          *
        // **************************************************************
        nN++; // Номер лексемы
        Lex(nType, cLex, nN, &nCommand);
        //
        if (nCommand != CMD_NONE) break;
        
        li = (LEXEMITEM *)(&cBuf[li->Next]);
     }
     //
     if (nCommand != CMD_CONT) {
       DropTo(0);
       ncLType = 0;
       //
       if ((ncOSP != 0)||(ncVSP > 1)) {
          RaiseError(1001, "Error in expression");
       }
       //
       if (ncStrInfo == DO_IF) {
          if (ncVSP != 1) {
             RaiseError(1002, "Error in IF expression");
          }
          //
          --ncVSP; // Эмулируем JF
          //
          if (lList) {
             sprintf(buf, "        [%8x] JF IF%04dX", pcCodeBuf.dwPos, ncCSP);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_JF);
          lStackClear = true;
          // AAdd(acCStack[ncCSP, CS_FIXUP], {pcCodeBuf.dwPos, 3}) // Привязка по ИНАЧЕ (или концу цикла)
          acCStack[ncCSP].FixupList[acCStack[ncCSP].FixupCnt].PC = pcCodeBuf.dwPos;
          acCStack[ncCSP].FixupList[acCStack[ncCSP].FixupCnt].Type = 3;
          ++acCStack[ncCSP].FixupCnt;
          PlaceOffset(0);
          //
       } else if (ncStrInfo == DO_WHILE) {
          //
          if (ncVSP != 1) {
             RaiseError(1003, "Error in WHILE expression");
          }
          //
          --ncVSP; // Эмулируем JF
          //
          if (lList) {
             sprintf(buf, "        [%8x] JF WH%04dE", pcCodeBuf.dwPos, ncCSP);
             WriteLst(buf);
          }
          //
          PlaceOp(OPC_JF);
          lStackClear = true;
          // AAdd(acCStack[ncCSP, CS_FIXUP], {pcCodeBuf.dwPos, 2}) // Привязка по концу цикла
          acCStack[ncCSP].FixupList[acCStack[ncCSP].FixupCnt].PC = pcCodeBuf.dwPos;
          acCStack[ncCSP].FixupList[acCStack[ncCSP].FixupCnt].Type = 2;
          ++acCStack[ncCSP].FixupCnt;
          PlaceOffset(0);
          //
       }
       //
       ncStrInfo = 0;
       ncOSP = 0;
       if (!lStackClear) {
       	  if (lList) {
       	     sprintf(buf, "        [%8x] POPV", pcCodeBuf.dwPos);
             WriteLst(buf);
          }
          PlaceOp(OPC_POPV);
       }
       while (ncVSP > 0) {
         if (acVarStack[ncVSP].Str) mmFree(acVarStack[ncVSP].Str);
         memset(&(acVarStack[ncVSP]), 0, sizeof(VARSTACKITEM));
         --ncVSP;
       }
       lcClear = true;
     }
     nCommand = CMD_NONE;
   }
   if (newsrc) continue;
   ncLineTotal += pSrc->Reader->Line();
   delete pSrc->Reader;
   memset(pSrc, 0, sizeof(SOURCEITEM));
   pSrc = 0;
   if (ncSource == 0) break;
   --ncSource;
  }
  ViewStatus();
  if (ncCSP != 0) {
     RaiseError(1004, "Unclosed control structures");
  }
  }
  catch (char *excmsg)
  {
     if (strcmp(excmsg, MDS_EXC_SIGNATURE) == 0) {
       if (ncError == 0) {
         ncError = 9999;
         StdErrMsg(ncError, "Unknown error occured while compilation");
       }
     } else {
       throw;
     }
  }
  if (hcLstFile) {
     CloseHandle(hcLstFile);
     hcLstFile = 0;
  }
  if (cBuf) {
    mmFree(cBuf);
    cBuf = 0;
  }
  if (ncError == 0) {
     //
     PlaceOp(OPC_QUIT);
     PlaceOp(0);
     //
     if ((MDSF_NOWAIT & dFlg) == 0) {
        sprintf(buf, "Compilation completed\r\rLines: %d Code: %d Symbols: %d", ncLineTotal, pcCodeBuf.dwPos, pcSymBuf.dwPos);
        if (pCallBack) {
          pCallBack(CB_MESSAGE, dwCBCtx, buf, (DWORD)hWnd);
        } else {
          MessageBox(0, buf, "Message", MB_OK | MB_TASKMODAL | MB_ICONINFORMATION);
        }
     }
     //
     DWORD ds = pcCodeBuf.dwPos;
     //
     rH = (MDS_HEADER *)(pcCodeBuf.pBuf);
     rH->dSignature = MDSH_SIGN;
     rH->dCodeOfs = sizeof(MDS_HEADER);
     while (ds & 0x0000000F) ++ds;
     rH->dSymTblOfs = ds;
     ds += pcSymBuf.dwPos;
     while (ds & 0x0000000F) ++ds;
     if ((dFlg & MDSF_DEBUGINFO) != 0) {
        rH->dDebugTblOfs = ds;
	      rH->dDebugTblLen = nDItem;
	      ds += (sizeof(DEBUGINFOITEM) * (rH->dDebugTblLen + 1));
	      while (ds & 0x0000000F) ++ds;
	      rH->dSrcListOfs = ds;
	      ds += pcSrcList.dwPos;
	      while ((ds & 0x0000000F) != 0) ++ds;
     } 
     rH->dSize = ds;
     pCode = (BYTE *)mmAlloc(ds);
     if (pCode) {
       memset(pCode, 0, ds);
       memcpy(pCode, pcCodeBuf.pBuf, pcCodeBuf.dwPos);
       if (pcSymBuf.dwPos) memcpy(&(pCode[rH->dSymTblOfs]), pcSymBuf.pBuf, pcSymBuf.dwPos);
       if ((dFlg & MDSF_DEBUGINFO) != 0) {
         if (rH->dDebugTblLen) memcpy(&(pCode[rH->dDebugTblOfs]), pDebugInfo, rH->dDebugTblLen * sizeof(DEBUGINFOITEM));
         if (pcSrcList.dwPos) memcpy(&(pCode[rH->dSrcListOfs]), pcSrcList.pBuf, pcSrcList.dwPos);
       }
       dwBufLen = ds; 
       dRet = ds;
       if (dFlg & MDSF_DUMP) {
         if (pCallBack) pCallBack(CB_INFO, dwCBCtx, "Creating dump...", (DWORD)hWnd);
         char f[_MAX_PATH];
         strcpy(f, filename);
         ReplaceExt(f, "dmp");
         mdsDisassemble(f, pCode);
       }
       
     } else {
       dRet = RET_FAILED;
     }
     //
  } else {
     dRet = (0x80000000 | ncError);
  }
  BufAlloc(&pcSymBuf, 0);
  BufAlloc(&pcCodeBuf, 0);
  BufAlloc(&pcSrcList, 0);
  if (pDebugInfo) {
     mmFree(pDebugInfo);
     pDebugInfo = 0;
  }
  return dRet;
}

// CCompiledCode
CCompiledCode::CCompiledCode(MDS_HEADER *code):CUsualObject(UCT_CODE)
{
  m_pData = 0;
  if (code->dSignature == MDSH_SIGN) m_pData = code;
}

CCompiledCode::~CCompiledCode()
{
  if (m_pData) mmFree(m_pData);
  m_pData = 0;
}

HRESULT CCompiledCode::ItemI(int action, int index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CCompiledCode::ItemS(int action, char *index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CCompiledCode::ItemU(int action, USUAL *index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CCompiledCode::FirstItem(USUAL *index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CCompiledCode::NextItem(USUAL *index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CCompiledCode::CallMethod(char *name, USUAL *result, int parcnt, USUAL *parlst)
{
  return E_FAIL;
}

HRESULT CCompiledCode::Sort(USUAL *param, bool desc)
{
  return E_FAIL;
}

int CCompiledCode::Length()
{
  if (m_pData) return ((MDS_HEADER *)m_pData)->dSize;
  return 0;
}

//
// uscript.cpp EOF
//

