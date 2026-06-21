//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

/*
//---------------------------------------------------------------------------
//=========================================================================

	TLCS900h_interpret.h

//=========================================================================
//---------------------------------------------------------------------------

  History of changes:
  ===================

20 JUL 2002 - neopop_uk
=======================================
- Cleaned and tidied up for the source release

21 JUL 2002 - neopop_uk
=======================================
- Added the 'instruction_error' function declaration here.

28 JUL 2002 - neopop_uk
=======================================
- Removed CYCLE_WARNING as it is now obsolete.
- Added generic DIV prototypes.

//---------------------------------------------------------------------------
*/

#ifndef __TLCS900H_INTERPRET__
#define __TLCS900H_INTERPRET__

#include <boolean.h>
#include "../../mednafen-types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================

//Interprets a single instruction from 'pc', 
//pc is incremented to the start of the next instruction.
//Returns the number of cycles taken for this instruction
int32 TLCS900h_interpret(void);

//=============================================================================

extern uint32 mem;	
extern int size;
extern uint8 first;			//First byte
extern uint8 second;			//Second byte
extern uint8 R;				//(second & 7)
extern uint8 rCode;
extern int32 cycles;
extern bool brCode;

//=============================================================================

static INLINE uint8 fetch8_fast(void)
{
	uint32_t addr;
	uint8_t *page;

	addr = pc++ & 0xFFFFFF;
	page = FastReadMap[addr >> 16];
	if (page)
		return page[addr];
	return loadB(addr);
}
#define FETCH8	fetch8_fast()

uint16 fetch16(void);
uint32 fetch24(void);
uint32 fetch32(void);

//=============================================================================

void parityB(uint8 value);
void parityW(uint16 value);

//=============================================================================

void push8(uint8 data);
void push16(uint16 data);
void push32(uint32 data);

uint8 pop8(void);
uint16 pop16(void);
uint32 pop32(void);

//=============================================================================

//DIV ===============
uint16 generic_DIV_B(uint16 val, uint8 div);
uint32 generic_DIV_W(uint32 val, uint16 div);

//DIVS ===============
uint16 generic_DIVS_B(int16 val, int8 div);
uint32 generic_DIVS_W(int32 val, int16 div);

//ADD ===============
static INLINE uint8 generic_ADD_B(uint8 dst, uint8 src)
{
	uint8 half = (dst & 0xF) + (src & 0xF);
	uint32 resultC = (uint32)dst + (uint32)src;
	uint8 result = (uint8)(resultC & 0xFF);

	SETFLAG_S(result & 0x80);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int8)dst >= 0) && ((int8)src >= 0) && ((int8)result < 0)) ||
		(((int8)dst < 0)  && ((int8)src < 0) && ((int8)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFF);

	return result;
}

static INLINE uint16 generic_ADD_W(uint16 dst, uint16 src)
{
	uint16 half = (dst & 0xF) + (src & 0xF);
	uint32 resultC = (uint32)dst + (uint32)src;
	uint16 result = (uint16)(resultC & 0xFFFF);

	SETFLAG_S(result & 0x8000);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int16)dst >= 0) && ((int16)src >= 0) && ((int16)result < 0)) ||
		(((int16)dst < 0)  && ((int16)src < 0) && ((int16)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFFFF);

	return result;
}

static INLINE uint32 generic_ADD_L(uint32 dst, uint32 src)
{
	uint64 resultC = (uint64)dst + (uint64)src;
	uint32 result = (uint32)(resultC & 0xFFFFFFFF);

	SETFLAG_S(result & 0x80000000);
	SETFLAG_Z(result == 0);

	if ((((int32)dst >= 0) && ((int32)src >= 0) && ((int32)result < 0)) ||
		(((int32)dst < 0)  && ((int32)src < 0) && ((int32)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFFFFFFFF);

	return result;
}

//ADC ===============
static INLINE uint8 generic_ADC_B(uint8 dst, uint8 src)
{
	uint8 half = (dst & 0xF) + (src & 0xF) + FLAG_C;
	uint32 resultC = (uint32)dst + (uint32)src + (uint32)FLAG_C;
	uint8 result = (uint8)(resultC & 0xFF);

	SETFLAG_S(result & 0x80);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int8)dst >= 0) && ((int8)src >= 0) && ((int8)result < 0)) ||
		(((int8)dst < 0)  && ((int8)src < 0) && ((int8)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFF);

	return result;
}

static INLINE uint16 generic_ADC_W(uint16 dst, uint16 src)
{
	uint16 half = (dst & 0xF) + (src & 0xF) + FLAG_C;
	uint32 resultC = (uint32)dst + (uint32)src + (uint32)FLAG_C;
	uint16 result = (uint16)(resultC & 0xFFFF);

	SETFLAG_S(result & 0x8000);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int16)dst >= 0) && ((int16)src >= 0) && ((int16)result < 0)) ||
		(((int16)dst < 0)  && ((int16)src < 0) && ((int16)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFFFF);

	return result;
}

static INLINE uint32 generic_ADC_L(uint32 dst, uint32 src)
{
	uint64 resultC = (uint64)dst + (uint64)src + (uint64)FLAG_C;
	uint32 result = (uint32)(resultC & 0xFFFFFFFF);

	SETFLAG_S(result & 0x80000000);
	SETFLAG_Z(result == 0);

	if ((((int32)dst >= 0) && ((int32)src >= 0) && ((int32)result < 0)) ||
		(((int32)dst < 0)  && ((int32)src < 0) && ((int32)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N0;
	SETFLAG_C(resultC > 0xFFFFFFFF);

	return result;
}

//SUB ===============
static INLINE uint8 generic_SUB_B(uint8 dst, uint8 src)
{
	uint8 half = (dst & 0xF) - (src & 0xF);
	uint32 resultC = (uint32)dst - (uint32)src;
	uint8 result = (uint8)(resultC & 0xFF);

	SETFLAG_S(result & 0x80);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int8)dst >= 0) && ((int8)src < 0) && ((int8)result < 0)) ||
		(((int8)dst < 0) && ((int8)src >= 0) && ((int8)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFF);

	return result;
}

static INLINE uint16 generic_SUB_W(uint16 dst, uint16 src)
{
	uint16 half = (dst & 0xF) - (src & 0xF);
	uint32 resultC = (uint32)dst - (uint32)src;
	uint16 result = (uint16)(resultC & 0xFFFF);

	SETFLAG_S(result & 0x8000);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int16)dst >= 0) && ((int16)src < 0) && ((int16)result < 0)) ||
		(((int16)dst < 0) && ((int16)src >= 0) && ((int16)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFFFF);

	return result;
}

static INLINE uint32 generic_SUB_L(uint32 dst, uint32 src)
{
	uint64 resultC = (uint64)dst - (uint64)src;
	uint32 result = (uint32)(resultC & 0xFFFFFFFF);

	SETFLAG_S(result & 0x80000000);
	SETFLAG_Z(result == 0);

	if ((((int32)dst >= 0) && ((int32)src < 0) && ((int32)result < 0)) ||
		(((int32)dst < 0) && ((int32)src >= 0) && ((int32)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFFFFFFFF);

	return result;
}

//SBC ===============
static INLINE uint8 generic_SBC_B(uint8 dst, uint8 src)
{
	uint8 half = (dst & 0xF) - (src & 0xF) - FLAG_C;
	uint32 resultC = (uint32)dst - (uint32)src - (uint32)FLAG_C;
	uint8 result = (uint8)(resultC & 0xFF);

	SETFLAG_S(result & 0x80);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int8)dst >= 0) && ((int8)src < 0) && ((int8)result < 0)) ||
		(((int8)dst < 0) && ((int8)src < 0) && ((int8)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFF);

	return result;
}

static INLINE uint16 generic_SBC_W(uint16 dst, uint16 src)
{
	uint16 half = (dst & 0xF) - (src & 0xF) - FLAG_C;
	uint32 resultC = (uint32)dst - (uint32)src - (uint32)FLAG_C;
	uint16 result = (uint16)(resultC & 0xFFFF);

	SETFLAG_S(result & 0x8000);
	SETFLAG_Z(result == 0);
	SETFLAG_H(half > 0xF);

	if ((((int16)dst >= 0) && ((int16)src < 0) && ((int16)result < 0)) ||
		(((int16)dst < 0) && ((int16)src < 0) && ((int16)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFFFF);

	return result;
}

static INLINE uint32 generic_SBC_L(uint32 dst, uint32 src)
{
	uint64 resultC = (uint64)dst - (uint64)src - (uint64)FLAG_C;
	uint32 result = (uint32)(resultC & 0xFFFFFFFF);

	SETFLAG_S(result & 0x80000000);
	SETFLAG_Z(result == 0);

	if ((((int32)dst >= 0) && ((int32)src < 0) && ((int32)result < 0)) ||
		(((int32)dst < 0) && ((int32)src < 0) && ((int32)result >= 0)))
	{SETFLAG_V1} else {SETFLAG_V0}

	SETFLAG_N1;
	SETFLAG_C(resultC > 0xFFFFFFFF);

	return result;
}

//=============================================================================

//Confirms a condition code check
bool conditionCode(int cc);

//=============================================================================

//Translate an rr or RR value for MUL/MULS/DIV/DIVS
uint8 get_rr_Target(void);
uint8 get_RR_Target(void);

#ifdef __cplusplus
}
#endif

//=============================================================================
#endif
