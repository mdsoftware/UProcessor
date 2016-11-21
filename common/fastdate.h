/******************************************************************

  File Name 
      FASTDATE.H

  Description
      Fast data processing.

  Author
      Denis M
*******************************************************************/

#ifndef _FASTDATE_H
#define _FASTDATE_H

#include <WINDOWS.H>
#include <time.h>

#pragma pack(push, 1)
typedef struct _FASTDATE
{
  union {
    LONGLONG DateTime;
    struct {
      long Time;
      long Date;
    } Value;
  };
} FASTDATE, *PFASTDATE;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct _UNPACKEDDATE
{
  short Year;
  unsigned char Month;
  unsigned char Day;
  unsigned char Hour;
  unsigned char Min;
  unsigned char Sec;
  short MSec;
} UNPACKEDDATE, *PUNPACKEDDATE;
#pragma pack(pop)

/**********************************************************
 Pack date from year, month and day
 Time from hour, min and sec
 Return S_OK if success, E_INVALIDARG or E_FAIL
 Year must be from 1 to 4096
 Month from 1 to 12
 Days range is accordingly to month
 E_INVALIDARG otherwise.
 Time in 23:59:59 limits.
***********************************************************/
HRESULT WINAPI fdPackDate(int year, int month, int day, FASTDATE *date);
HRESULT WINAPI fdPackTime(int hour, int min, int sec, FASTDATE *date);

/**********************************************************
 Convert date from FASTDATE to UNPACKEDDATE.
 Returns S_OK if all is ok, E_INVALIDARG otherwise.
 If process failed, dest content remains unchanged.
***********************************************************/
HRESULT WINAPI fdUnpackDate(FASTDATE *src, UNPACKEDDATE *dest);

/**********************************************************
Check if year is leap
***********************************************************/
bool WINAPI fdIsLeap(FASTDATE *d);

/**********************************************************
Return day of week, -1 if error
0 - Sunday
1 - Monday
2 - Tuesday
3 - Wednesday
4 - Thursday
5 - Friday
6 - Saturday
***********************************************************/
int WINAPI fdDayOfWeek(FASTDATE *d);

/**********************************************************
Compare two dates
if x > y return 1
if x = y return 0
if x < y return -1
***********************************************************/
int WINAPI fdCmpDates(FASTDATE *x, FASTDATE *y);

/**********************************************************
Add two dates
if result = NULL, x = x + y else result = x + y
***********************************************************/
HRESULT WINAPI fdAddDate(FASTDATE *result, FASTDATE *x, FASTDATE *y);

/**********************************************************
Subtract two dates
if result = NULL, x = x - y else result = x - y
***********************************************************/
HRESULT WINAPI fdSubDate(FASTDATE *result, FASTDATE *x, FASTDATE *y);

/**********************************************************
Return current date and time
***********************************************************/
HRESULT WINAPI fdCurDate(FASTDATE *d);

#endif

