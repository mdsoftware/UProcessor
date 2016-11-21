/******************************************************************

  File Name 
      SRCFILEREADER.CPP

  Description
      Reads specified source file char by char with a very high speed.

  Author
      Denis M
*******************************************************************/

#include "stdafx.h"
#include "srcfilereader.h"

// CSrcReader
CSrcReader::CSrcReader()
{
  hFile = 0;
  pBuf = 0;
  iLineCount = 0;
  Close();
}

void CSrcReader::Close()
{
  if (hFile) {
    CloseHandle(hFile);
  }
  if (pBuf) {
    mmFree(pBuf);
  }
  hFile = 0;
  bEof = false;
  bLastBuf = false;
  Ch = 0;
  NextCh = 0;
  pBuf = 0;
  dwBufSize = 0;
  dwBufPos = 0;
  cErrMsg[0] = 0;
  cSrcFile[0] = 0;
  iLineCount = 0;
}

HRESULT CSrcReader::Open(char *fname)
{
   Close();
   strcpy(cSrcFile, fname);
   HRESULT hR = S_OK;
   iLineCount = 0;
   iExpLine = 1;
   iExpType = EXPT_NONE;
   cSavedCh = 0;
   hFile = CreateFile(cSrcFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	 if (hFile == INVALID_HANDLE_VALUE) {
	   hR = GetLastError();
	   hFile = 0;
	   wsprintf(cErrMsg, "Error opening input file %s :0x%08x (%d)", fname, hR, hR);
	   return hR;
	 };
   pBuf = (char *)mmAlloc(SRC_BUFSIZE);
   if (pBuf == 0) {
     strcpy(cErrMsg, "Error allocating input file buffer");
     return E_OUTOFMEMORY;
   }
   hR = ReadBuffer();
   if (hR) return hR;
   if (bEof) return hR;
   NextCh = pBuf[0];
   return S_OK;
}

HRESULT CSrcReader::NextChar()
{
   if (bEof) return E_FAIL;
   Ch = NextCh;
   NextCh = 0;
   ++dwBufPos;
   if (dwBufPos >= dwBufSize) {
     if (bLastBuf) {
        bEof = true;
        return S_OK; 
     }
     HRESULT hR = ReadBuffer();
     if (hR) return hR;
     if (bEof) return S_OK;
     NextCh = pBuf[0];
   } else {
     NextCh = pBuf[dwBufPos];
   }
   return S_OK;
}

HRESULT CSrcReader::ReadBuffer()
{
   if ((hFile == 0) || (pBuf == 0)) return E_FAIL;
   DWORD read = 0;
   if (!ReadFile(hFile, (void *)pBuf, SRC_BUFSIZE, &read, 0)) {
     HRESULT hR = GetLastError();
     wsprintf(cErrMsg, "Error reading source file :0x%08x (%d)", hR, hR);
     return hR;
   }
   dwBufSize = read;
   dwBufPos = 0;
   bLastBuf = (dwBufSize < SRC_BUFSIZE);
   if (bLastBuf) {
     CloseHandle(hFile);
     hFile = 0;
   }
   bEof = (bLastBuf && (dwBufPos >= dwBufSize));
   return S_OK;
}

bool CSrcReader::Eof()
{
   return bEof;
}

int CSrcReader::Line()
{
   return iLineCount;
}

int CSrcReader::ExpLine()
{
   return iExpLine;
}

HRESULT CSrcReader::NextLine(char *buf, int buflen)
{
   int l = 0;
   buf[0] = 0;
   HRESULT r = S_OK;
   if (Eof()) return E_FAIL;
   while (l < buflen) {
     r = NextChar();
     if (r) return r;
     if ((Ch == 13) && (NextCh == 10)) {
       r = NextChar();
       if (r) return r;
       buf[l] = 0;
       ++iLineCount;
       return S_OK;
     }
     buf[l] = Ch;
     ++l;
     if (Eof()) {
       buf[l] = 0;
       ++iLineCount;
       return S_OK; 
     }
   }
   return E_FAIL;
}

#define EXPT_STR1     10 // '
#define EXPT_STR2     11 // "
#define EXPT_REMARK   12
#define EXPT_REMEOL   13

HRESULT CSrcReader::NextExpr(char *buf, int buflen, int *explen, int *exptype)
{
   HRESULT r = S_OK;
   if (Eof()) return E_FAIL;
   int type = iExpType;
   if (type == EXPT_NONE) type = EXPT_TEXT;
   int l = 0;
   bool lasteoe = false;
   *exptype = EXPT_NONE;
   *explen = 0;
   if ((type == EXPT_CODE) && (cSavedCh)) {
     buf[l] = cSavedCh;
     ++l;
     cSavedCh = 0;
   } else {
     buf[l] = 0;
   }
   iExpLine = iLineCount + 1;
   while (true) {
     r = NextChar();
     if (r) return r;
     if (type == EXPT_STR1) {
       if (Ch == '\'') type = EXPT_CODE;
       if (l >= (buflen - 8)) return E_FAIL;
       buf[l] = Ch;
       ++l;
     } else if (type == EXPT_STR2) {
       if (Ch == '"') type = EXPT_CODE;
       if (l >=  (buflen - 8)) return E_FAIL;
       buf[l] = Ch;
       ++l;
     } else if (type == EXPT_REMARK) {
       if (((Ch == 13) && (NextCh == 10)) || (Ch == 10)) ++iLineCount;
       if ((Ch == '*') && (NextCh == '/')) {
         r = NextChar();
         if (r) return r;
         type = EXPT_CODE;
       }
     } else if (type == EXPT_REMEOL) {
       if (((Ch == 13) && (NextCh == 10)) || (Ch == 10)) {
         if (Ch == 13) {
           r = NextChar();
           if (r) return r;
         }
         ++iLineCount;
         type = EXPT_CODE;
         if (lasteoe) {
           lasteoe = false;
           continue;
         }
         lasteoe = false;
         if (l == 0) continue;
         *exptype = EXPT_CODE;
         iExpType = EXPT_CODE;
         break;
       }
     } else if (type == EXPT_CODE) {
       if ((Ch == '$') && (NextCh == '>')) {
         r = NextChar();
         if (r) return r;
         *exptype = EXPT_CODE;
         iExpType = EXPT_TEXT;
         break;
       }
       if ((Ch == '/') && (NextCh == '*')) {
         type = EXPT_REMARK;
         continue;
       }
       if ((Ch == '/') && (NextCh == '/')) {
         type = EXPT_REMEOL;
         continue;
       }
       if (((Ch == 13) && (NextCh == 10)) || (Ch == 10)) {
         if (Ch == 13) {
           r = NextChar();
           if (r) return r;
         }
         ++iLineCount;
         if (lasteoe) {
           lasteoe = false;
           continue;
         }
         lasteoe = false;
         if (l == 0) continue;
         *exptype = EXPT_CODE;
         iExpType = EXPT_CODE;
         break;
       }
       if (Ch == ';') {
         lasteoe = true;
         continue;
       }
       if ((Ch == ' ') || (Ch == 9)) {
         if ((l > 0) && (buf[l-1] != ' ')) {
           if (l >= (buflen - 8)) return E_FAIL;
           buf[l] = ' ';
           ++l;
         }
       } else {
         if (lasteoe && (l > 0)) {
           cSavedCh = Ch;
           *exptype = EXPT_CODE;
           iExpType = EXPT_CODE;
           break;
         }
         if (l == 0) iExpLine = iLineCount + 1;
         if (l == buflen) return E_FAIL;
         buf[l] = Ch;
         ++l;
         if (Ch == '\'') type = EXPT_STR1;
         else if (Ch == '"') type = EXPT_STR2;
       } 
     } else if (type == EXPT_TEXT) {
       if (((Ch == 13) && (NextCh == 10)) || (Ch == 10)) {
         if (Ch == 13) {
           buf[l] = Ch;
           ++l;
           r = NextChar();
           if (r) return r;
         }
         ++iLineCount;
       }
       if ((Ch == '<') && (NextCh == '$')) {
         r = NextChar();
         if (r) return r;
         type = EXPT_CODE;
         if (l == 0) continue;
         *exptype = EXPT_TEXT;
         iExpType = EXPT_CODE;
         break;
       }
       buf[l] = Ch;
       ++l;
       if (l >= (buflen - 8)) {
         *exptype = EXPT_TEXT;
         iExpType = EXPT_TEXT;
         break;
       };
     }
     if (Eof()) {
       *exptype = type;
       iExpType = EXPT_NONE;
       break; 
     }
   }
   if (r == S_OK) {
     *explen = l;
     buf[l] = 0;
   }
   return r;
}

CSrcReader::~CSrcReader()
{
  Close();
}


//
// SRCFILEREADER.CPP EOF
//

