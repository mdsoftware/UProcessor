/******************************************************************

  File Name 
      ULISTS.CPP

  Description
      Main USUAL collections.

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"
#include "ulists.h"

// CUsualRecord
CUsualRecord::CUsualRecord(int startsize):CUsualObject(UCT_RECORD)
{
  m_iLast = 0;
  if (startsize == 0) 
    m_iSize = DEF_RECORD_STARTSIZE; else
    m_iSize = startsize;
  while (m_iSize & 0x7) ++m_iSize;
  m_iLength = 0;
  m_Items = 0;
  m_iCurItem = -1;
}

CUsualRecord::~CUsualRecord()
{
  if (m_Items) {
    if (m_iLength) {
      for (int i = 0; i < m_iLast; i++) {
        if (m_Items[i].Name[0] != 0) {
          UClear(&(m_Items[i].Value));
        }
      }
    }
    mmFree(m_Items);
    m_Items = 0;
  }
  m_iLast = 0;
  m_iSize = 0;
  m_iLength = 0;
}

URECORDITEM *CUsualRecord::NewItem()
{
  m_iCurItem = -1;
  if (m_Items == 0) {
    DWORD s = m_iSize * sizeof(URECORDITEM);
    URECORDITEM *p = (URECORDITEM *)mmAlloc(s);
    if (p == 0) return 0;
    memset(p, 0, s);
    m_Items = p;
    ++m_iLast;
    ++m_iLength;
    return &(m_Items[0]);
  }
  if (m_iLast < m_iSize) {
    int i = m_iLast;
    ++m_iLast;
    ++m_iLength;
    return &(m_Items[i]);
  }
  if (m_iLength < m_iLast) {
    for (int i = 0; i < m_iLast; i++) {
      if (m_Items[i].Name[0] == 0) {
         ++m_iLength;
         return &(m_Items[i]);
      }
    }
  }
  int n = m_iSize + 8;
  DWORD s = n * sizeof(URECORDITEM);
  URECORDITEM *p = (URECORDITEM *)mmAlloc(s);
  if (p == 0) return 0;
  memset(p, 0, s);
  if (m_iLast) memcpy(p, m_Items, m_iLast * sizeof(URECORDITEM));
  mmFree(m_Items);
  m_Items = p;
  m_iSize = n;
  return NewItem();
}

static void CopyRecName(char *dest, char *src)
{
  int l = (int)strlen(src);
  if (l > RECNAME_MAX) l = RECNAME_MAX;
  memcpy(dest, src, l);
  while (l <= RECNAME_MAX) {
    dest[l] = 0; 
    ++l;
  }
  _strlwr(dest);
}

static bool CmpRecName(char *x, char *y)
{
  for (int i = 0; i < RECNAME_MAX; i++) {
    if (x[i] != y[i]) return false;
  }
  return true;
}

URECORDITEM *CUsualRecord::FindItem(char *name, bool add)
{
  if (m_iLast) {
    char x[RECNAME_MAX + 1];
    CopyRecName(x, name);
    for (int i = 0; i < m_iLast; i++) {
      if (m_Items[i].Name[0]) {
        if (CmpRecName(m_Items[i].Name, x)) {
          return &(m_Items[i]);
        }
      } 
    }
  }
  if (!add) return 0;
  URECORDITEM *p = NewItem();
  if (p == 0) return 0;
  CopyRecName(&(p->Name[0]), name);
  p->Value.ulTag = MAKE_TAG(UT_NIL);
  return p;
}

int CUsualRecord::Length()
{
  return m_iLength;
}

static HRESULT CopyItemTo(USUAL *dest, USUAL *src)
{
  if (dest == 0) return E_INVALIDARG;
  if (TAG_TYPE(dest->ulTag) == UT_REF) {
    dest->Value.ref = src;
    return S_OK;
  };
  return UAssign(dest, src); 
}

HRESULT CUsualRecord::FirstItem(USUAL *index, USUAL *item)
{
  m_iCurItem = 0;
  return NextItem(index, item);
}

HRESULT CUsualRecord::NextItem(USUAL *index, USUAL *item)
{
  if ((m_iCurItem < 0) || (m_iCurItem >= m_iLast)) {
    m_iCurItem = -1;
    return E_FAIL;
  }
  while (m_iCurItem < m_iLast) {
    if (m_Items[m_iCurItem].Name[0] != 0) break;
    ++m_iCurItem;
  }
  if (m_iCurItem >= m_iLast) {
    m_iCurItem = -1;
    return E_FAIL;
  }
  URECORDITEM *p = &(m_Items[m_iCurItem]);
  if (index) {
    if (USetStrA(index, p->Name)) {
      m_iCurItem = -1;
      return E_FAIL;
    }
  }
  if (CopyItemTo(item, &(p->Value))) {
    m_iCurItem = -1;
    return E_FAIL;
  }
  ++m_iCurItem;
  return S_OK;
}

HRESULT CUsualRecord::ItemI(int action, int index, USUAL *item)
{
  if (action == ITEM_SET) {
    if (item == 0) return E_INVALIDARG;
    URECORDITEM *p = GetItem(index);
    if (p == 0) return E_FAIL;
    return UAssign(&(p->Value), item);
  }
  if (action == ITEM_GET) {
    if (item == 0) return E_INVALIDARG;
    URECORDITEM *p = GetItem(index);
    if (p == 0) return E_FAIL;
    return CopyItemTo(item, &(p->Value));
  }
  if (action == ITEM_REMOVE) {
    URECORDITEM *p = GetItem(index);
    if (p == 0) return E_FAIL;
    UClear(&(p->Value));
    memset(p, 0, sizeof(URECORDITEM));
    --m_iLength;
    return S_OK;
  }
  return E_INVALIDARG;
}

HRESULT CUsualRecord::ItemS(int action, char *index, USUAL *item)
{
  if (action == ITEM_SET) {
    if (item == 0) return E_INVALIDARG;
    URECORDITEM *p = FindItem(index, true);
    if (p == 0) return E_OUTOFMEMORY;
    return UAssign(&(p->Value), item);
  }
  if (action == ITEM_GET) {
    if (item == 0) return E_INVALIDARG;
    URECORDITEM *p = FindItem(index, false);
    if (p == 0) return E_FAIL;
    return CopyItemTo(item, &(p->Value));
  }
  if (action == ITEM_REMOVE) {
    URECORDITEM *p = FindItem(index, false);
    if (p == 0) return E_FAIL;
    UClear(&(p->Value));
    memset(p, 0, sizeof(URECORDITEM));
    --m_iLength;
    return S_OK;
  }
  return E_INVALIDARG;
}

HRESULT CUsualRecord::ItemU(int action, USUAL *index, USUAL *item)
{
  if (index == 0) return E_INVALIDARG;
  if (TAG_TYPE(index->ulTag) == UT_INT) {
    return ItemI(action, index->Value.l, item);
  }
  if (TAG_TYPE(index->ulTag) == UT_STR_A) {
    return ItemS(action, index->Value.strA.str, item);
  }
  return E_INVALIDARG;
}

HRESULT CUsualRecord::Sort(USUAL *param, bool desc)
{
  return E_FAIL;
}

URECORDITEM *CUsualRecord::GetItem(int index)
{
  if ((index < 1) || (index > m_iLength)) return 0;
  int i = 0;
  int c = 0;
  while (i < m_iLast) {
    if (m_Items[i].Name[0]) {
      ++c;
      if (c == index) return &(m_Items[i]);
    }
    ++i;
  }
  return 0;
}

char *CUsualRecord::GetFldName(int index)
{
  URECORDITEM *p = GetItem(index);
  if (p == 0) return 0;
  return p->Name;
}

// CUsualArray
CUsualArray::CUsualArray(int startsize):CUsualObject(UCT_ARRAY)
{
  m_PageList = 0;
  m_iLength = startsize;
  int initpages = 0;
  if (m_iLength) {
    initpages = ARRAY_PAGENUM(m_iLength) + 1;
  }
  m_iMaxPage = ARRAY_MINPAGE;  
  if (m_iMaxPage < initpages) {
    m_iMaxPage = initpages;
    while (m_iMaxPage & 0xf) ++m_iMaxPage;
  }
  m_PageList = (USUAL **)mmAlloc(m_iMaxPage * sizeof(USUAL*));
  if (m_PageList) {
    memset(m_PageList, 0, m_iMaxPage * sizeof(USUAL*));
    if (m_iLength) {
      for (int i = 1; i <= m_iLength; i++) {
        USUAL *u = GetItemPtr(i);
        if (u) UClear(u);
      }
    }
  } else {
    m_iMaxPage = 0;
    m_iLength = 0;
  }
  m_iCurItem = -1;
}

CUsualArray::~CUsualArray()
{
  if (m_PageList) {
    if (m_iLength) {
      for (int i = 1; i <= m_iLength; i++) {
        USUAL *u = GetItemPtr(i);
        if (u) UClear(u);
      }
    }
    for (int i = 0; i < m_iMaxPage; i++) {
      if (m_PageList[i]) mmFree(m_PageList[i]);
    }
    mmFree(m_PageList);
  }
  m_PageList = 0;
  m_iMaxPage = 0;
  m_iLength = 0;
  m_iCurItem = -1;
}

USUAL *CUsualArray::GetPagePtr(int index)
{
  int page = ARRAY_PAGENUM(index-1);
  if (page >= m_iMaxPage) {
    int maxpage = page + 1;
    while (maxpage & 0xf) ++maxpage;
    USUAL **p = (USUAL **)mmAlloc(maxpage * sizeof(USUAL*));
    if (p == 0) return 0;
    memset(p, 0, maxpage * sizeof(USUAL*));
    if (m_iMaxPage) memcpy(p, m_PageList, m_iMaxPage * sizeof(USUAL*));
    if (m_PageList) mmFree(m_PageList);
    m_iMaxPage = maxpage;
    m_PageList = p;
  }
  if (m_PageList[page] == 0) {
    USUAL *p = (USUAL *)mmAlloc(ARRAY_PAGESIZE * sizeof(USUAL));
    if (p == 0) return 0;
    memset(p, 0, ARRAY_PAGESIZE * sizeof(USUAL));
    m_PageList[page] = p;
  }
  return m_PageList[page];
}

USUAL *CUsualArray::GetItemPtr(int index)
{
  USUAL *page = GetPagePtr(index);
  if (page == 0) return 0;
  int i = ARRAY_PAGEITEM(index-1);
  if ((i < 0) || (i > ARRAY_PAGEMASK)) {
    MessageBox(0, "!!!!!", "!!!!!", MB_OK | MB_TASKMODAL);
  }
  return &(page[ARRAY_PAGEITEM(index-1)]);
}

USUAL *CUsualArray::AddItem(int index)
{
  if ((index < 1) || (index > (m_iLength + 1))) return 0;
  ++m_iLength;
  if (index < m_iLength) {
    int i = m_iLength - 1;
    while (i >= index) {
      USUAL *x = GetItemPtr(i+1);
      USUAL *y = GetItemPtr(i);
      if ((x) && (y)) *x = *y;
      --i;
    }
  }
  USUAL *u = GetItemPtr(index);
  if (u == 0) return 0;
  memset(u, 0, sizeof(USUAL));
  u->ulTag = MAKE_TAG(UT_NIL);
  return u;
}

bool CUsualArray::DelItem(int index)
{
  if ((index < 1) || (index > m_iLength)) return false;
  USUAL *u = GetItemPtr(index);
  if (u == 0) return false;
  UClear(u);
  --m_iLength;
  if (index <= m_iLength) {
    int i = index;
    while (i <= m_iLength) {
      USUAL *x = GetItemPtr(i);
      USUAL *y = GetItemPtr(i+1);
      if ((x) && (y)) *x = *y;
      ++i;
    }
  }
  u = GetItemPtr(m_iLength+1);
  if (u) memset(u, 0, sizeof(USUAL));
  return true;
}

int CUsualArray::Length()
{
  return m_iLength;
}

HRESULT CUsualArray::ItemI(int action, int index, USUAL *item)
{
  if (action == ITEM_SET) {
    if (item == 0) return E_INVALIDARG;
    if ((index < 1) || (index > (m_iLength + 1))) return E_FAIL;
    if (index > m_iLength) {
      if (AddItem(index) == 0) return E_FAIL;
    }
    USUAL *u = GetItemPtr(index);
    if (u == 0) return E_FAIL;
    return UAssign(u, item);
  }
  if (action == ITEM_GET) {
    if (item == 0) return E_INVALIDARG;
    if ((index < 1) || (index > m_iLength)) return E_FAIL;
    USUAL *u = GetItemPtr(index);
    if (u == 0) return E_FAIL;
    return CopyItemTo(item, u);
  }
  if (action == ITEM_REMOVE) {
    if ((index < 1) || (index > m_iLength)) return E_FAIL;
    if (DelItem(index)) return S_OK;
    return E_FAIL;
  }
  return E_INVALIDARG;
}

HRESULT CUsualArray::ItemS(int action, char *index, USUAL *item)
{
  return E_FAIL;
}

HRESULT CUsualArray::ItemU(int action, USUAL *index, USUAL *item)
{
  if (index == 0) return E_INVALIDARG;
  if (TAG_TYPE(index->ulTag) == UT_INT) {
    return ItemI(action, index->Value.l, item);
  }
  return E_INVALIDARG;
}

HRESULT CUsualArray::FirstItem(USUAL *index, USUAL *item)
{
  m_iCurItem = 1;
  return NextItem(index, item);
}

HRESULT CUsualArray::NextItem(USUAL *index, USUAL *item)
{
  if ((m_iCurItem < 1) || (m_iCurItem > m_iLength)) {
    m_iCurItem = -1;
    return E_FAIL;
  }
  USUAL *p = GetItemPtr(m_iCurItem);
  if (p == 0) return E_FAIL;
  if (index) {
    if (USetInt(index, m_iCurItem)) {
      m_iCurItem = -1;
      return E_FAIL;
    }
  }
  if (CopyItemTo(item, p)) {
    m_iCurItem = -1;
    return E_FAIL;
  }
  ++m_iCurItem;
  return S_OK;
}

HRESULT CUsualArray::CallMethod(char *name, USUAL *result, int parcnt, USUAL *parlst)
{
  if (SymCmp(name, "insert")) {
    if (parcnt != 2) return E_INVALIDARG;
    if (PARAM(1).ulTag != MAKE_TAG(UT_INT)) return E_INVALIDARG;
    USUAL *u = AddItem(PARAM(1).Value.l);
    if (u == 0) return E_FAIL;
    return UAssign(u, &(PARAM(2)));
  } else if (SymCmp(name, "delete")) {
    if (parcnt != 1) return E_INVALIDARG;
    if (PARAM(1).ulTag != MAKE_TAG(UT_INT)) return E_INVALIDARG;
    if (DelItem(PARAM(1).Value.l)) return S_OK;
    return E_FAIL;
  }
  return CUsualObject::CallMethod(name, result, parcnt, parlst);
}

static HRESULT WINAPI SortCompare(USUAL *x, USUAL *y, USUAL *param, bool desc, int *result)
{
  *result = 0;
  if ((x == 0) || (y == 0)) return E_FAIL;
  HRESULT r = S_OK;
  if (param == 0) {
    if (desc) {
      r = UCompare(y, x, result);
    } else {
      r = UCompare(x, y, result);
    }
    return r;
  }
  if (x->ulTag != MAKE_TAG(UT_OBJECT)) return E_FAIL;
  if (x->Value.obj == 0) return E_FAIL;
  if (y->ulTag != MAKE_TAG(UT_OBJECT)) return E_FAIL;
  if (y->Value.obj == 0) return E_FAIL;
  USUAL ux = {0};
  USUAL uy = {0};
  r = x->Value.obj->ItemU(ITEM_GET, param, &ux);
  if (r) return r;
  r = y->Value.obj->ItemU(ITEM_GET, param, &uy);
  if (r) {
     UClear(&ux);
     return r;
  }
  if (desc) {
    r = UCompare(&uy, &ux, result);
  } else {
    r = UCompare(&ux, &uy, result);
  }
  UClear(&ux);
  UClear(&uy);
  return r;
}

static HRESULT WINAPI QuickSort(CUsualArray *arr, int L, int R, USUAL *param, bool desc)
{
  int I; int J;
  USUAL P = {0};
  HRESULT r = S_OK;
  while (true) {
    I = L;
    J = R;
    USUAL *u = arr->GetItemPtr(((L + R) >> 1) + 1);
    if (u == 0) {
      UClear(&P);
      return E_FAIL;
    }
    r = UAssign(&P, u);
    if (r) return r;
    while (true) {
      while (true) {
        int x = 0;
        r = SortCompare(arr->GetItemPtr(I+1), &P, param, desc, &x);
        if (r) {
          UClear(&P);
          return r;
        }
        if (x >= 0) break;
        I++;
      }
      while (true) {
        int x = 0;
        r = SortCompare(arr->GetItemPtr(J+1), &P, param, desc, &x);
        if (r) {
          UClear(&P);
          return r;
        }
        if (x <= 0) break;
        J--;
      }
      if (I <= J) {
        if (I != J) {
          USUAL T;
          USUAL *x = arr->GetItemPtr(I+1);
          USUAL *y = arr->GetItemPtr(J+1);
          if ((x == 0) || (y == 0)) {
            UClear(&P);
            return E_FAIL;
          }
          T = *x;
          *x = *y;
          *y = T;
        }
        I++;
        J--;
      };
      if (I > J) break;
    }
    if (L < J) {
      r = QuickSort(arr, L, J, param, desc);
      if (r) {
        UClear(&P);
        return r;
      }
    }
    L = I;
    if (I >= R) break;
  }
  UClear(&P);
  return r;
}

HRESULT CUsualArray::Sort(USUAL *param, bool desc)
{
  if (m_iLength <= 1) return S_OK;
  return QuickSort(this, 0, m_iLength - 1, param, desc);
}

//
// ulists.cpp EOF
//

