//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version. See also the license.txt file for
//      additional informations.
//---------------------------------------------------------------------------

#include "../mednafen-types.h"
#include <time.h>

static uint8 rtc_latch[7];

static void update_rtc_latch(void)
{
   uint8 low, high;
   time_t t;
   struct tm *lt;
   int year, mon, mday, hour, min, sec, wday;

   t = time(NULL);
   lt = localtime(&t);

   if (lt)
   {
      year = lt->tm_year;
      mon  = lt->tm_mon;
      mday = lt->tm_mday;
      hour = lt->tm_hour;
      min  = lt->tm_min;
      sec  = lt->tm_sec;
      wday = lt->tm_wday;
   }
   else
   {
      /* RTC unavailable — use safe defaults (2000-01-01 00:00:00 Sat) */
      year = 100; mon = 0; mday = 1;
      hour = 0; min = 0; sec = 0; wday = 6;
   }

   low = year - 100; high = low;
   rtc_latch[0x00] = ((high / 10) << 4) | (low % 10);

   low = mon + 1; high = low;
   rtc_latch[0x01] = ((high / 10) << 4) | (low % 10);

   low = mday; high = low;
   rtc_latch[0x02] = ((high / 10) << 4) | (low % 10);

   low = hour; high = low;
   rtc_latch[0x03] = ((high / 10) << 4) | (low % 10);

   low = min; high = low;
   rtc_latch[0x04] = ((high / 10) << 4) | (low % 10);

   low = sec; high = low;
   rtc_latch[0x05] = ((high / 10) << 4) | (low % 10);

   rtc_latch[0x06] = ((rtc_latch[0x00] % 4)<<4) | (wday & 0x0F);
}

uint8 rtc_read8(uint32 address)
{
   if(address >= 0x0091 && address <= 0x0097)
   {
      if(address == 0x0091)
         update_rtc_latch();

      return rtc_latch[address - 0x0091];
   }
   return 0;
}
