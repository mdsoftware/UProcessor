/******************************************************************

  File Name 
      USUAL.H

  Description
      Generic definitions for USUAL variables.

  Author
      Denis M
*******************************************************************/

#ifndef _USUAL_H
#define _USUAL_H

#include <WINDOWS.H>
#include "fastdate.h"


// #define PROTECTED_LIB

// ANSI string for USUAL
#pragma pack(push, 1)
typedef struct _USTRA
{
  char *str;
  int len;
} USTRA;
#pragma pack(pop)

// WIDE string for USUAL
#pragma pack(push, 1)
typedef struct _USTRW
{
  WCHAR *str;
  int len;
} USTRW;
#pragma pack(pop)

#define USUAL_SIGN        ('UX' << 16)
#define USUAL_SIGN_MASK   0xffff0000
#define USUAL_TYPE_MASK   0x0000ffff

#define MAKE_TAG(type) ((USUAL_SIGN & USUAL_SIGN_MASK) | (type))
#define TAG_TYPE(t) ((t) & USUAL_TYPE_MASK)

// Usual variable types
#define UT_NIL      0x0000

// Types which not require dynamic cleanup
#define UT_INT      0x0001
// #define UT_INT64    0x0002 // Not used yet...
#define UT_DOUBLE   0x0003
#define UT_LOGIC    0x0004
#define UT_DATETIME 0x0005
#define UT_PTR      0x0006
#define UT_CHAR     0x0007
#define UT_WCHAR    0x0008
#define UT_REF      0x0009
#define UT_SYMBOL   0x000a

// Types which require dynamic cleanup
#define UT_MASK_DYNAMIC 0x00f0
#define UT_STR_A        0x0010
#define UT_STR_W        0x0020
#define UT_CODE         0x0030

// Types which require object Release()
#define UT_MASK_OBJECT  0x0f00
#define UT_OBJECT       0x0100

// Item actions (action parameter in ItemX methods)
#define ITEM_SET        0x1
#define ITEM_GET        0x2
#define ITEM_REMOVE     0x3

class CUsualObject;

/******************************************************

General definition for USUAL variable

*******************************************************/
#pragma pack(push, 1)
typedef struct _USUAL
{
  union {
    long l;             // UT_INT
    double dbl;         // UT_DOUBLE
    bool b;             // UT_LOGIC
    char ch;            // UT_CHAR
    WCHAR wch;          // UT_WCHAR
    void *ptr;          // UT_PTR
    char *sym;          // UT_SYMBOL
    FASTDATE fd;        // UT_DATETIME
    USTRA strA;         // UT_STR_A
    USTRW strW;         // UT_STR_W
    CUsualObject *obj;  // UT_OBJECT
    _USUAL *ref;         // UT_REF
  
  } Value;
  unsigned long ulTag;
  unsigned long ulCargo;
} USUAL, *PUSUAL;
#pragma pack(pop)

/******************************************************
 Clear USUAL variable
*******************************************************/
HRESULT WINAPI UClear(USUAL *var);

/******************************************************
 Assign src to dest
*******************************************************/
HRESULT WINAPI UAssign(USUAL *dest, USUAL *src);

/******************************************************
 Set USUAL to ANSI string with length (-1) means
 we need to use strlen
*******************************************************/
HRESULT WINAPI USetStrA(USUAL *var, char *str, int len = -1);
HRESULT WINAPI USetStrW(USUAL *var, WCHAR *str, int len = -1);

/******************************************************
 Set USUAL to integer (long)
*******************************************************/
HRESULT WINAPI USetInt(USUAL *var, long value);

/******************************************************
 Set USUAL to double
*******************************************************/
HRESULT WINAPI USetFloat(USUAL *var, double value);

/******************************************************
 Set USUAL to bool
*******************************************************/
HRESULT WINAPI USetLogic(USUAL *var, bool value);

/******************************************************
 Set USUAL to object (calling AddRef)
*******************************************************/
HRESULT WINAPI USetObj(USUAL *var, CUsualObject *value);

/******************************************************
 Set USUAL to FASTDATE
*******************************************************/
HRESULT WINAPI USetDate(USUAL *var, FASTDATE *value);

/******************************************************
 Compare two items, objects are compared using Length()
 if x > y result = 1
 if x = y result = 0
 if x < y result = -1
 if return is not S_OK result is undefined
*******************************************************/
HRESULT WINAPI UCompare(USUAL *x, USUAL *y, int *result);

/******************************************************
 Add two USUALs
 if result = 0 then x = x + y else result = x + y
*******************************************************/
HRESULT WINAPI UAdd(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Subtract two USUALs
 if result = 0 then x = x - y else result = x - y
*******************************************************/
HRESULT WINAPI USub(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Multiply two USUALs
 if result = 0 then x = x * y else result = x * y
*******************************************************/
HRESULT WINAPI UMul(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Divide two USUALs
 if result = 0 then x = x / y else result = x / y
*******************************************************/
HRESULT WINAPI UDiv(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Get negative value of USUAL
 if result = 0 then x = -x else result = -x
*******************************************************/
HRESULT WINAPI UNeg(USUAL *result, USUAL *x);

/******************************************************
 Detects if USUAL is true
*******************************************************/
HRESULT WINAPI UIsTrue(USUAL *x, bool *result);

/******************************************************
 And's two USUALs
 if result = 0 then x = x & y else result = x & y
*******************************************************/
HRESULT WINAPI UAnd(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Or's two USUALs
 if result = 0 then x = x | y else result = x | y
*******************************************************/
HRESULT WINAPI UOr(USUAL *result, USUAL *x, USUAL *y);

/******************************************************
 Get NOT value of USUAL
 if result = 0 then x = !x else result = !x
*******************************************************/
HRESULT WINAPI UNot(USUAL *result, USUAL *x);

// Parameter processing, since parameters are in a reverse
// order in stack
#define PARAM(n) parlst[parcnt-n]

// Code to return when default call is required
#define E_USEDEFAULT    ((HRESULT)0xffffffff)
#define E_BREAK         ((HRESULT)0xfffffffe)
#define E_OK_FALSE      ((HRESULT)0xfffffffd)

/******************************************************

Base class for all objects which can be used as
UT_OBJECT

*******************************************************/
class CUsualObject
{
public:
  int AddRef();
  int Release();
  int Type();
  void *Data();
  
// Pure virtual members

  // Item functions to operate Items
  // accordingly with it's index and action (ITEM_XXX)
  // result is used only with UTEM_GET
  // All integer indexes are started from 1
  virtual HRESULT ItemI(int action, int index, USUAL *item = 0) = 0;
  virtual HRESULT ItemS(int action, char *index, USUAL *item = 0) = 0;
  virtual HRESULT ItemU(int action, USUAL *index, USUAL *item = 0) = 0;
  
  // Returns first and then next and next items from
  // object. If error or no more items available
  // then return E_FAIL
  // index can be 0, it means that no index will be returned
  virtual HRESULT FirstItem(USUAL *index, USUAL *item) = 0;
  virtual HRESULT NextItem(USUAL *index, USUAL *item) = 0;
  
  // Calls object method
  virtual HRESULT CallMethod(char *name, USUAL *result, int parcnt, USUAL *parlst);
  
  // Sort the object
  virtual HRESULT Sort(USUAL *param, bool desc = false) = 0;
  
  // Return available length (if applicable)
  virtual int Length() = 0;
protected:
  bool SymCmp(char *sym, char *str);
public:
  CUsualObject(int type, void *data = 0);
  virtual ~CUsualObject();
protected:
  int m_iType;
  int m_iRefCnt;
  void *m_pData;
  USUAL m_CurIndex;
  USUAL m_Current;
  CUsualObject *m_Prev;
  CUsualObject *m_Next;
};

#endif

