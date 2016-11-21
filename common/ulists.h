/******************************************************************

  File Name 
      ULISTS.H

  Description
      Main USUAL collections.

  Author
      Denis M
*******************************************************************/

#ifndef _ULISTS_H
#define _ULISTS_H

#include <WINDOWS.H>
#include "usual.h"

// Types of collections
#define UCT_RECORD      0x1
#define UCT_ARRAY       0x2
#define UCT_COLLECTION  0x3
#define UCT_CODE        0x4

#define RECNAME_MAX           63
#define DEF_RECORD_STARTSIZE  8

#pragma pack(push, 1)
typedef struct _URECORDITEM
{
  char Name[RECNAME_MAX + 1];
  USUAL Value;
} URECORDITEM, *PURECORDITEM;
#pragma pack(pop)


/******************************************************
Record object, which holds USUAL values against the
name of the value. Name is ANSI string within 31 char
length. Names are case insensitive.
*******************************************************/
class CUsualRecord: public CUsualObject
{
private:
  URECORDITEM *NewItem();
  URECORDITEM *FindItem(char *name, bool add = false);
  URECORDITEM *GetItem(int index);
public:
  // Item functions to operate Items
  // accordingly with it's index and action (ITEM_XXX)
  // result is used only with UTEM_GET
  HRESULT ItemI(int action, int index, USUAL *item = 0);
  HRESULT ItemS(int action, char *index, USUAL *item = 0);
  HRESULT ItemU(int action, USUAL *index, USUAL *item = 0);
  HRESULT FirstItem(USUAL *index, USUAL *item);
  HRESULT NextItem(USUAL *index, USUAL *item);
  HRESULT Sort(USUAL *param, bool desc = false);
  
  // Return available length (if applicable)
  int Length();
  
  // Return name of the field
  char *GetFldName(int index);

public:
  CUsualRecord(int startsize = 0);
  ~CUsualRecord();
protected:
  URECORDITEM *m_Items;
  int m_iLast;
  int m_iSize;  
  int m_iLength;
  int m_iCurItem;
};

#define ARRAY_PAGEMASK    0x03ff
#define ARRAY_PAGESIZE    (ARRAY_PAGEMASK+1)
#define ARRAY_PAGENUM(a)  ((a) >> 10) // 1111111111
#define ARRAY_PAGEITEM(a) ((a) & ARRAY_PAGEMASK)
#define ARRAY_MINPAGE     32

/******************************************************
Record object, which holds USUAL values against the
index of the value. Index is integer value starts from 1.
*******************************************************/
class CUsualArray: public CUsualObject
{
private:
  USUAL *AddItem(int index);
  bool DelItem(int index);
  USUAL *GetPagePtr(int index);
  
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
  
  // Return available length (if applicable)
  int Length();
  
  // For sorting issues
  USUAL *GetItemPtr(int index);

public:
  CUsualArray(int startsize = 0);
  ~CUsualArray();
protected:
  USUAL **m_PageList;
  int m_iMaxPage;  
  int m_iLength;
  int m_iCurItem;  
};

#endif

