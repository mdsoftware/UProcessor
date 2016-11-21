/******************************************************************

  File Name 
      SRCFILEREADER.H

  Description
      Reads specified source file char by char with a very high speed.

  Author
      Denis M
*******************************************************************/

#ifndef _SRCFILEREADER_H
#define _SRCFILEREADER_H

#include "memmgr.h"

#define SRC_BUFSIZE 0x0000ffff
#define EXPRLEN_MAX 4095

// Types of expression
#define EXPT_NONE   0
#define EXPT_CODE   1
#define EXPT_TEXT   2

class CSrcReader
{
protected:
  HANDLE hFile;
  bool bEof;
  bool bLastBuf;
  char *pBuf;
  DWORD dwBufSize;
  DWORD dwBufPos;
  int iLineCount;
  int iExpType;
  char cSavedCh;
  int iExpLine;
  
  HRESULT ReadBuffer();
  
public:  
  char Ch;
  char NextCh;
  char cErrMsg[512];
  char cSrcFile[512];

  void Close();  
  HRESULT Open(char *fname);
  HRESULT NextChar();
  bool Eof();
  int Line();
  int ExpLine();
  HRESULT NextLine(char *buf, int buflen);
  HRESULT NextExpr(char *buf, int buflen, int *explen, int *exptype);

public:
  CSrcReader();
  ~CSrcReader();
};

#endif

