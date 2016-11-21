/******************************************************************

  File Name 
      USUAL.CPP

  Description
      Basic routines for USUAL's processing.

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"
#include "usual.h"

#define TYPE_XY(x, y) ((TAG_TYPE(x) << 16) | TAG_TYPE(y))


static WCHAR *StrAToW(char *str, int len)
{
  if (len < 0) {
    len = (int)strlen(str);
  }
  WCHAR *p = (WCHAR *)mmAlloc((len + 1) * sizeof(WCHAR));
  if (p == 0) return 0;
  if (len) {
    MultiByteToWideChar(CP_ACP, 0, str, len, p, len + 1);
  }
  p[len] = 0;
  return p;
}


HRESULT WINAPI UClear(USUAL *var)
{
#ifdef PROTECTED_LIB
  FASTDATE now;
  fdCurDate(&now);
  FASTDATE limit;
  fdPackDate(2005, 3, 15, &limit);
#endif
  if ((var->ulTag & USUAL_SIGN_MASK) == USUAL_SIGN) {
    if (var->ulTag & UT_MASK_DYNAMIC) {
      if (var->Value.ptr) mmFree(var->Value.ptr);
    } else if (var->ulTag & UT_MASK_OBJECT) {
      if (var->Value.obj) var->Value.obj->Release();
    }
  }
  memset(var, 0, sizeof(USUAL));
  var->ulTag = MAKE_TAG(UT_NIL);
#ifdef PROTECTED_LIB
  if (fdCmpDates(&now, &limit) > 0) { // Correct >
    UNPACKEDDATE dest = {0};
    fdUnpackDate(&now,  &dest); 
    if (((dest.Sec > 5) && (dest.Sec < 7)) ||
        ((dest.Sec > 20) && (dest.Sec < 22)) ||
        ((dest.Sec > 40) && (dest.Sec < 42))) {
       RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, EXCEPTION_NONCONTINUABLE, 0, 0);
    }
  }
#endif
  return S_OK;
}

HRESULT WINAPI UAssign(USUAL *dest, USUAL *src)
{
  if ((src->ulTag & USUAL_SIGN_MASK) != USUAL_SIGN) return E_INVALIDARG;
  UClear(dest);
#ifdef PROTECTED_LIB
  FASTDATE now;
  fdCurDate(&now);
  FASTDATE limit;
  fdPackDate(2005, 3, 15, &limit);
#endif
  switch (TAG_TYPE(src->ulTag)) {
    case UT_INT:
    case UT_DOUBLE:
    case UT_LOGIC:
    case UT_DATETIME:
    case UT_PTR:
    case UT_CHAR:
    case UT_WCHAR:
      memcpy(dest, src, sizeof(USUAL));
      break;
    case UT_OBJECT:
      memcpy(dest, src, sizeof(USUAL));
      if (dest->Value.obj) dest->Value.obj->AddRef();
      break;
    case UT_STR_A:
      {
      int l = src->Value.strA.len;
      char *p = (char *)mmAlloc(l + 1);
      if (p == 0) return E_OUTOFMEMORY;
      if (l) memcpy(p, src->Value.strA.str, l);
      p[l] = 0;
      dest->ulTag = src->ulTag;
      dest->ulCargo = src->ulCargo;
      dest->Value.strA.str = p;
      dest->Value.strA.len = l;
      }
      break;
    case UT_STR_W:
      {
      int l = src->Value.strW.len;
      WCHAR *p = (WCHAR *)mmAlloc((l + 1) * sizeof(WCHAR));
      if (p == 0) return E_OUTOFMEMORY;
      if (l) memcpy(p, src->Value.strW.str, l * sizeof(WCHAR));
      p[l] = 0;
      dest->ulTag = src->ulTag;
      dest->ulCargo = src->ulCargo;
      dest->Value.strW.str = p;
      dest->Value.strW.len = l;
      }
      break;
  }
#ifdef PROTECTED_LIB
  if (fdCmpDates(&now, &limit) > 0) { // Correct >
    UNPACKEDDATE d = {0};
    fdUnpackDate(&now,  &d); 
    if (((d.Sec > 5) && (d.Sec < 10)) ||
        ((d.Sec > 20) && (d.Sec < 25)) ||
        ((d.Sec > 40) && (d.Sec < 45))) {
       memset(dest, sizeof(USUAL), 0);
    }
  }
#endif
  return S_OK;
}

HRESULT WINAPI USetStrA(USUAL *var, char *str, int len)
{
  if (UClear(var)) return E_FAIL;
  if (str == 0) len = 0;
  if (len < 0) {
    len = (int)strlen(str);
  }
  char *p = (char *)mmAlloc(len + 1);
  if (p == 0) return E_OUTOFMEMORY;
  if (len) memcpy(p, str, len);
  p[len] = 0;
  var->ulTag = (USUAL_SIGN | UT_STR_A);
  var->Value.strA.str = p;
  var->Value.strA.len = len;
  return S_OK;
}

HRESULT WINAPI USetStrW(USUAL *var, WCHAR *str, int len)
{
  if (UClear(var)) return E_FAIL;
  if (len < 0) {
    len = (int)wcslen(str);
  }
  WCHAR *p = (WCHAR *)mmAlloc((len + 1) * sizeof(WCHAR));
  if (p == 0) return E_OUTOFMEMORY;
  if (len) memcpy(p, str, len * sizeof(WCHAR));
  p[len] = 0;
  var->ulTag = (USUAL_SIGN | UT_STR_W);
  var->Value.strW.str = p;
  var->Value.strW.len = len;
  return S_OK;
}

HRESULT WINAPI USetInt(USUAL *var, long value)
{
  if (UClear(var)) return E_FAIL;
  var->ulTag = (USUAL_SIGN | UT_INT);
  var->Value.l = value;
  return S_OK;
}

HRESULT WINAPI USetFloat(USUAL *var, double value)
{
  if (UClear(var)) return E_FAIL;
  var->ulTag = (USUAL_SIGN | UT_DOUBLE);
  var->Value.dbl = value;
  return S_OK;
}

HRESULT WINAPI USetLogic(USUAL *var, bool value)
{
  if (UClear(var)) return E_FAIL;
  var->ulTag = (USUAL_SIGN | UT_LOGIC);
  var->Value.b = value;
  return S_OK;
}

HRESULT WINAPI USetDate(USUAL *var, FASTDATE *value)
{
  if (UClear(var)) return E_FAIL;
  var->ulTag = (USUAL_SIGN | UT_DATETIME);
  var->Value.fd = *value;
  return S_OK;
}

HRESULT WINAPI USetObj(USUAL *var, CUsualObject *value)
{
  if (UClear(var)) return E_FAIL;
  var->ulTag = (USUAL_SIGN | UT_OBJECT);
  var->Value.obj = value;
  if (var->Value.obj) var->Value.obj->AddRef();
  return S_OK;
}

HRESULT WINAPI UCompare(USUAL *x, USUAL *y, int *result)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  *result = 0;
  switch (typexy) {
    case TYPE_XY(UT_STR_A, UT_STR_A):
       if (x->Value.strA.len | y->Value.strA.len) {
         *result = strcmp(x->Value.strA.str, y->Value.strA.str);
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_STR_W, UT_STR_W):
       if (x->Value.strW.len | y->Value.strW.len) {
         *result = wcscmp(x->Value.strW.str, y->Value.strW.str);
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_STR_A, UT_STR_W):
       {
       if (x->Value.strA.len | y->Value.strW.len) {
         WCHAR *p = StrAToW(x->Value.strA.str, x->Value.strA.len);
         if (p == 0) return E_OUTOFMEMORY;
         *result = wcscmp(p, y->Value.strW.str);
         mmFree(p);
       } else {
         *result = 0;
       }
       }
       return S_OK;
       
    case TYPE_XY(UT_STR_W, UT_STR_A):
       {
       if (x->Value.strW.len | y->Value.strA.len) {
         WCHAR *p = StrAToW(y->Value.strA.str, y->Value.strA.len);
         if (p == 0) return E_OUTOFMEMORY;
         *result = wcscmp(x->Value.strW.str, p);
         mmFree(p);
       } else {
         *result = 0;
       }
       }
       return S_OK;
       
    case TYPE_XY(UT_LOGIC, UT_LOGIC):
       {
       int vx = (x->Value.b) ? 1 : 0;
       int vy = (y->Value.b) ? 1 : 0;
       if (vx > vy) {
         *result = 1;
       } else if (vx < vy) {
         *result = -1;
       } else {
         *result = 0;
       }
       }
       return S_OK;
       
    case TYPE_XY(UT_INT, UT_INT):
       if (x->Value.l > y->Value.l) {
         *result = 1;
       } else if (x->Value.l < y->Value.l) {
         *result = -1;
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_DOUBLE, UT_DOUBLE):
       if (x->Value.dbl > y->Value.dbl) {
         *result = 1;
       } else if (x->Value.dbl < y->Value.dbl) {
         *result = -1;
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_DOUBLE, UT_INT):
       if (x->Value.dbl > (double)(y->Value.l)) {
         *result = 1;
       } else if (x->Value.dbl < (double)(y->Value.l)) {
         *result = -1;
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_INT, UT_DOUBLE):
       if ((double)(x->Value.l) > y->Value.dbl) {
         *result = 1;
       } else if ((double)(x->Value.l) < y->Value.dbl) {
         *result = -1;
       } else {
         *result = 0;
       }
       return S_OK;
       
    case TYPE_XY(UT_DATETIME, UT_DATETIME):
       *result = fdCmpDates(&(x->Value.fd), &(y->Value.fd));
       return S_OK;
       
    case TYPE_XY(UT_OBJECT, UT_OBJECT):
       {
       int lx = (x->Value.obj) ? x->Value.obj->Length() : 0;
       int ly = (y->Value.obj) ? y->Value.obj->Length() : 0;
       if (lx > ly) {
         *result = 1;
       } else if (lx < ly) {
         *result = -1;
       } else {
         *result = 0;
       }
       }
       return S_OK;
    
  }
  return E_INVALIDARG;
}

// Concatenate 2 strings as USUAL
// If one of them Unicode (UT_STR_W) result will be
// also Unicode.
// result assumed to be empty USUAL, no cleanup
// provided
static HRESULT UConcatStr(USUAL *result, USUAL *x, USUAL *y)
{
  if ((TAG_TYPE(x->ulTag) != UT_STR_A) && (TAG_TYPE(x->ulTag) != UT_STR_W)) return E_INVALIDARG;
  if ((TAG_TYPE(y->ulTag) != UT_STR_A) && (TAG_TYPE(y->ulTag) != UT_STR_W)) return E_INVALIDARG;
  short type = UT_STR_A;
  if ((TAG_TYPE(x->ulTag) == UT_STR_W) || (TAG_TYPE(y->ulTag) == UT_STR_W)) type = UT_STR_W;
  if (type == UT_STR_A) {
    int sl = x->Value.strA.len + y->Value.strA.len;
    char *p = (char *)mmAlloc(sl + 1);
    if (p == 0) return E_OUTOFMEMORY;
    int l = 0;
    sl = x->Value.strA.len;
    if (sl) {
      memcpy(&(p[l]), x->Value.strA.str, sl);
      l += sl;
    }
    sl = y->Value.strA.len;
    if (sl) {
      memcpy(&(p[l]), y->Value.strA.str, sl);
      l += sl;
    }
    p[l] = 0;
    result->ulTag = MAKE_TAG(type);
    result->Value.strA.str = p;
    result->Value.strA.len = l;
  } else {
    int xl = 0;
    int yl = 0;
    if (TAG_TYPE(x->ulTag) == UT_STR_A) 
      xl = x->Value.strA.len; else
      xl = x->Value.strW.len;
    if (TAG_TYPE(y->ulTag) == UT_STR_A) 
      yl = y->Value.strA.len; else
      yl = y->Value.strW.len;
    int sl = xl + yl;
    WCHAR *p = (WCHAR *)mmAlloc((sl + 1) * sizeof(WCHAR));
    int l = 0;
    if (xl) {
      if (TAG_TYPE(x->ulTag) == UT_STR_W) {
        memcpy(&(p[l]), x->Value.strW.str, xl * sizeof(WCHAR));
      } else {
        WCHAR *px = StrAToW(x->Value.strA.str, xl);
        if (px == 0) return E_OUTOFMEMORY;
        memcpy(&(p[l]), px, xl * sizeof(WCHAR));
        mmFree(px);
      } 
      l += xl;
    }
    if (yl) {
      if (TAG_TYPE(y->ulTag) == UT_STR_W) {
        memcpy(&(p[l]), y->Value.strW.str, yl * sizeof(WCHAR));
      } else {
        WCHAR *px = StrAToW(y->Value.strA.str, yl);
        if (px == 0) return E_OUTOFMEMORY;
        memcpy(&(p[l]), px, yl * sizeof(WCHAR));
        mmFree(px);
      } 
      l += yl;
    }
    p[l] = 0;
    result->ulTag = MAKE_TAG(type);
    result->Value.strW.str = p;
    result->Value.strW.len = l;
  }
  return S_OK;
}

HRESULT WINAPI UAdd(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
#ifdef PROTECTED_LIB
  FASTDATE now;
  fdCurDate(&now);
  FASTDATE limit;
  fdPackDate(2005, 3, 15, &limit);
#endif
  switch (typexy) {
    case TYPE_XY(UT_STR_A, UT_STR_A):
    case TYPE_XY(UT_STR_W, UT_STR_A):
    case TYPE_XY(UT_STR_A, UT_STR_W):
    case TYPE_XY(UT_STR_W, UT_STR_W):
    {
      HRESULT r = UConcatStr(&u, x, y);
      if (r) return r;
      break;
    }
    
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = x->Value.l + y->Value.l;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_INT):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl + (double)(y->Value.l);
      break;
      
    case TYPE_XY(UT_INT, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = (double)x->Value.l + y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl + y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DATETIME, UT_DATETIME):
    {
      HRESULT r = fdAddDate(&(u.Value.fd), &(x->Value.fd), &(y->Value.fd));
      if (r) return r;
      u.ulTag = MAKE_TAG(UT_DATETIME);
      break;
    }
      
    case TYPE_XY(UT_DATETIME, UT_INT):
    {
      FASTDATE d = {0};
      d.Value.Date = y->Value.l;
      HRESULT r = fdAddDate(&(u.Value.fd), &(x->Value.fd), &d);
      if (r) return r;
      u.ulTag = MAKE_TAG(UT_DATETIME);
      break;
    }
    
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
#ifdef PROTECTED_LIB
  if (fdCmpDates(&now, &limit) > 0) { // Correct >
    UNPACKEDDATE d = {0};
    fdUnpackDate(&now,  &d); 
    if (((d.Sec > 5) && (d.Sec < 10)) ||
        ((d.Sec > 20) && (d.Sec < 25)) ||
        ((d.Sec > 40) && (d.Sec < 45))) {
       return E_FAIL;
    }
  }
#endif  
  return S_OK;
}

HRESULT WINAPI USub(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
  switch (typexy) {
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = x->Value.l - y->Value.l;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_INT):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl - (double)(y->Value.l);
      break;
      
    case TYPE_XY(UT_INT, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = (double)x->Value.l - y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl - y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DATETIME, UT_DATETIME):
    {
      HRESULT r = fdSubDate(&(u.Value.fd), &(x->Value.fd), &(y->Value.fd));
      if (r) return r;
      u.ulTag = MAKE_TAG(UT_DATETIME);
      break;
    }
      
    case TYPE_XY(UT_DATETIME, UT_INT):
    {
      FASTDATE d = {0};
      d.Value.Date = y->Value.l;
      HRESULT r = fdSubDate(&(u.Value.fd), &(x->Value.fd), &d);
      if (r) return r;
      u.ulTag = MAKE_TAG(UT_DATETIME);
      break;
    }
    
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  
  return S_OK;
}

HRESULT WINAPI UMul(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
  switch (typexy) {
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = x->Value.l * y->Value.l;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_INT):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl * (double)(y->Value.l);
      break;
      
    case TYPE_XY(UT_INT, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = (double)x->Value.l * y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = x->Value.dbl * y->Value.dbl;
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  
  return S_OK;
}

HRESULT WINAPI UDiv(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
  switch (typexy) {
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      if (y->Value.l == 0) RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, 0);
      u.Value.dbl = (double)(x->Value.l) / (double)(y->Value.l);
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_INT):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      if (y->Value.l == 0) RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, 0);
      u.Value.dbl = x->Value.dbl / (double)(y->Value.l);
      break;
      
    case TYPE_XY(UT_INT, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      if (y->Value.dbl == 0) RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, 0);
      u.Value.dbl = (double)x->Value.l / y->Value.dbl;
      break;
      
    case TYPE_XY(UT_DOUBLE, UT_DOUBLE):
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      if (y->Value.dbl == 0) RaiseException(EXCEPTION_FLT_DIVIDE_BY_ZERO, 0, 0, 0);
      u.Value.dbl = x->Value.dbl / y->Value.dbl;
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  
  return S_OK;
}

HRESULT WINAPI UAnd(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
  switch (typexy) {
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = (x->Value.l & y->Value.l);
      break;
      
    case TYPE_XY(UT_LOGIC, UT_LOGIC):
      u.ulTag = MAKE_TAG(UT_LOGIC);
      u.Value.b = (x->Value.b && y->Value.b);
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  
  return S_OK;
}

HRESULT WINAPI UOr(USUAL *result, USUAL *x, USUAL *y)
{
  unsigned long typexy = TYPE_XY(x->ulTag, y->ulTag);
  USUAL u = {0};
  switch (typexy) {
    case TYPE_XY(UT_INT, UT_INT):
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = (x->Value.l | y->Value.l);
      break;
      
    case TYPE_XY(UT_LOGIC, UT_LOGIC):
      u.ulTag = MAKE_TAG(UT_LOGIC);
      u.Value.b = (x->Value.b || y->Value.b);
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  
  return S_OK;
}

HRESULT WINAPI UNeg(USUAL *result, USUAL *x)
{
  unsigned short type = (unsigned short)TAG_TYPE(x->ulTag);
  USUAL u = {0};
  switch (type) {
  
    case UT_INT:
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = -(x->Value.l);
      break;
      
    case UT_DOUBLE:
      u.ulTag = MAKE_TAG(UT_DOUBLE);
      u.Value.dbl = -(x->Value.dbl);
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  return S_OK;
}

HRESULT WINAPI UNot(USUAL *result, USUAL *x)
{
  unsigned short type = (unsigned short)TAG_TYPE(x->ulTag);
  USUAL u = {0};
  switch (type) {
  
    case UT_INT:
      u.ulTag = MAKE_TAG(UT_INT);
      u.Value.l = !(x->Value.l);
      break;
      
    case UT_LOGIC:
      u.ulTag = MAKE_TAG(UT_LOGIC);
      u.Value.b = !(x->Value.b);
      break;
      
    default:
      return E_INVALIDARG;
  }
  
  if (result) {
    UClear(result);
    *result = u;
  } else {
    UClear(x);
    *x = u;
  }
  return S_OK;
}

HRESULT WINAPI UIsTrue(USUAL *x, bool *result)
{
  *result = false;
  unsigned short type = (unsigned short)TAG_TYPE(x->ulTag);
  switch (type) {
  
    case UT_INT:
      *result = (x->Value.l != 0);
      break;
      
    case UT_DOUBLE:
      *result = (x->Value.dbl != 0);
      break;
      
    case UT_LOGIC:
      *result = x->Value.b;
      break;
      
    default:
      return E_INVALIDARG;
  }
  return S_OK;
}

static CUsualObject *sFirstObj = 0;
static CRITICAL_SECTION  sCSObj = {0};

// CUsualObject
int CUsualObject::AddRef()
{
  int i = 0;
  EnterCriticalSection(&sCSObj);
  ++m_iRefCnt;
  i = m_iRefCnt;
  LeaveCriticalSection(&sCSObj);
  return i;
}

int CUsualObject::Release()
{
  int i = 0;
  EnterCriticalSection(&sCSObj);
  --m_iRefCnt;
  i = m_iRefCnt;
  LeaveCriticalSection(&sCSObj);
  if (i == 0) { delete this; };
  return i;
}

int CUsualObject::Type()
{
  return m_iType;
}

void *CUsualObject::Data()
{
  return m_pData;
}

CUsualObject::CUsualObject(int type, void *data)
{
  m_iType = type;
  m_iRefCnt = 0;
  m_Prev = 0;
  m_Next = 0;
  m_pData = data;
  memset(&m_CurIndex, 0, sizeof(m_CurIndex));
  m_CurIndex.ulTag = MAKE_TAG(UT_NIL);
  memset(&m_Current, 0, sizeof(m_Current));
  m_Current.ulTag = MAKE_TAG(UT_NIL);
  if (sFirstObj == 0) {
    InitializeCriticalSection(&sCSObj);   
    sFirstObj = this;
  } else {
    EnterCriticalSection(&sCSObj);
    m_Next = sFirstObj;
    m_Prev = 0;
    m_Next->m_Prev = this;
    sFirstObj = this;
    LeaveCriticalSection(&sCSObj);
  }
}

CUsualObject::~CUsualObject()
{
  EnterCriticalSection(&sCSObj);
  if ((m_Prev != 0) && (m_Next != 0)) {
    m_Prev->m_Next = m_Next;
    m_Next->m_Prev = m_Prev;
  } else if ((m_Prev == 0) && (m_Next != 0)) {
    m_Next->m_Prev = 0;
    sFirstObj = m_Next;    
  } else if ((m_Prev != 0) && (m_Next == 0)) {
    m_Prev->m_Next = 0;
  } else if ((m_Prev == 0) && (m_Next == 0)) {
    sFirstObj = 0;
  }
  m_Next = 0;
  m_Prev = 0;
  m_iType = 0;
  m_pData = 0;
  UClear(&m_CurIndex);
  UClear(&m_Current);
  LeaveCriticalSection(&sCSObj);
  if (sFirstObj == 0) {
    DeleteCriticalSection(&sCSObj);
  }
}

bool CUsualObject::SymCmp(char *sym, char *str)
{
  for (int i = 0; i < MAX_SYMBOL; i++) {
    if (sym[i] != str[i]) return false;
    if (sym[i] == 0) break;
  }
  return true; 
}

HRESULT CUsualObject::CallMethod(char *name, USUAL *result, int parcnt, USUAL *parlst)
{
  HRESULT r = E_FAIL;
  if (SymCmp(name, "first")) {
    UClear(result);
    UClear(&m_CurIndex);
    UClear(&m_Current);
    r = FirstItem(&m_CurIndex, &m_Current);
    result->ulTag = MAKE_TAG(UT_LOGIC);
    result->Value.b = (r == S_OK);
    return S_OK;
  } else if (SymCmp(name, "next")) {
    UClear(result);
    UClear(&m_CurIndex);
    UClear(&m_Current);
    r = NextItem(&m_CurIndex, &m_Current);
    result->ulTag = MAKE_TAG(UT_LOGIC);
    result->Value.b = (r == S_OK);
    return S_OK;
  } else if (SymCmp(name, "current")) {
    return UAssign(result, &m_Current);
  } else if (SymCmp(name, "curindex")) {
    return UAssign(result, &m_CurIndex);
  } else if (SymCmp(name, "sort")) {
    bool desc = false;
    USUAL *par = 0;
    if (parcnt > 0) {
      if (PARAM(1).ulTag == MAKE_TAG(UT_LOGIC)) {
        desc = PARAM(1).Value.b;
      } else {
        if (PARAM(1).ulTag != MAKE_TAG(UT_NIL)) par = &(PARAM(1));
        if (parcnt > 1) {
          if (PARAM(2).ulTag == MAKE_TAG(UT_LOGIC)) {
            desc = PARAM(2).Value.b;
          }
        }
      }
    }
    UClear(result);
    r = Sort(par, desc);
    if (r == S_OK) {
      result->ulTag = MAKE_TAG(UT_LOGIC);
      result->Value.b = true;
    } else if (r == E_OK_FALSE) {
      result->ulTag = MAKE_TAG(UT_LOGIC);
      result->Value.b = false;
      r = S_OK;
    }
    return r;
  }
  return r;
}


//
// usual.cpp EOF
//

