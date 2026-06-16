/* TLCS-900h Threaded Interpreter with computed-goto primary dispatch.
 *
 * Compile this file instead of TLCS900h_interpret.c when GCC's
 * labels-as-values extension is available (SH-4/Dreamcast etc.).
 *
 * This file #includes the original TLCS900h_interpret.c to get
 * secondary dispatch tables, helper functions, and utility code.
 * Parts guarded by #ifndef THREADED_INTERPRETER (decodeExtra, decode
 * tables, Ex* helpers, sub-decoders, original TLCS900h_interpret) are
 * excluded and replaced with computed-goto versions below.
 */

#define THREADED_INTERPRETER

#include "TLCS900h_registers.h"
#include "../mem.h"
#include "../bios.h"
#include "TLCS900h_interpret.h"
#include "TLCS900h_interpret_single.h"
#include "TLCS900h_interpret_src.h"
#include "TLCS900h_interpret_dst.h"
#include "TLCS900h_interpret_reg.h"

/* Include handler implementations and secondary dispatch tables */
#include "TLCS900h_interpret_single.c"
#include "TLCS900h_interpret_src.c"
#include "TLCS900h_interpret_dst.c"
#include "TLCS900h_interpret_reg.c"
#include "TLCS900h_interpret.c"

/* Computed-goto interpreter — replaces the guarded TLCS900h_interpret() */
int32 TLCS900h_interpret(void)
{
	static const void * const extra_labels[256] = {
		/*0x00-0x7F: no extra decode */
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,&&xa_nop,
		/*0x80-0xBF: register indirect / displacement */
		&&xa_XWA,&&xa_XBC,&&xa_XDE,&&xa_XHL,&&xa_XIX,&&xa_XIY,&&xa_XIZ,&&xa_XSP,
		&&xa_XWAd,&&xa_XBCd,&&xa_XDEd,&&xa_XHLd,&&xa_XIXd,&&xa_XIYd,&&xa_XIZd,&&xa_XSPd,
		&&xa_XWA,&&xa_XBC,&&xa_XDE,&&xa_XHL,&&xa_XIX,&&xa_XIY,&&xa_XIZ,&&xa_XSP,
		&&xa_XWAd,&&xa_XBCd,&&xa_XDEd,&&xa_XHLd,&&xa_XIXd,&&xa_XIYd,&&xa_XIZd,&&xa_XSPd,
		&&xa_XWA,&&xa_XBC,&&xa_XDE,&&xa_XHL,&&xa_XIX,&&xa_XIY,&&xa_XIZ,&&xa_XSP,
		&&xa_XWAd,&&xa_XBCd,&&xa_XDEd,&&xa_XHLd,&&xa_XIXd,&&xa_XIYd,&&xa_XIZd,&&xa_XSPd,
		&&xa_XWA,&&xa_XBC,&&xa_XDE,&&xa_XHL,&&xa_XIX,&&xa_XIY,&&xa_XIZ,&&xa_XSP,
		&&xa_XWAd,&&xa_XBCd,&&xa_XDEd,&&xa_XHLd,&&xa_XIXd,&&xa_XIYd,&&xa_XIZd,&&xa_XSPd,
		/*0xC0-0xFF: immediate / special addressing */
		&&xa_8,   &&xa_16,  &&xa_24,  &&xa_R32, &&xa_Dec, &&xa_Inc, &&xa_nop, &&xa_RC,
		&&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop,
		&&xa_8,   &&xa_16,  &&xa_24,  &&xa_R32, &&xa_Dec, &&xa_Inc, &&xa_nop, &&xa_RC,
		&&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop,
		&&xa_8,   &&xa_16,  &&xa_24,  &&xa_R32, &&xa_Dec, &&xa_Inc, &&xa_nop, &&xa_RC,
		&&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop,
		&&xa_8,   &&xa_16,  &&xa_24,  &&xa_R32, &&xa_Dec, &&xa_Inc, &&xa_nop, &&xa_nop,
		&&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop, &&xa_nop
	};

	static const void * const op_labels[256] = {
		&&op_sngNOP,&&op_sngNORMAL,&&op_sngPUSHSR,&&op_sngPOPSR,
		&&op_sngMAX,&&op_sngHALT,&&op_sngEI,&&op_sngRETI,
		&&op_sngLD8_8,&&op_sngPUSH8,&&op_sngLD8_16,&&op_sngPUSH16,
		&&op_sngINCF,&&op_sngDECF,&&op_sngRET,&&op_sngRETD,
		&&op_sngRCF,&&op_sngSCF,&&op_sngCCF,&&op_sngZCF,
		&&op_sngPUSHA,&&op_sngPOPA,&&op_sngEX,&&op_sngLDF,
		&&op_sngPUSHF,&&op_sngPOPF,&&op_sngJP16,&&op_sngJP24,
		&&op_sngCALL16,&&op_sngCALL24,&&op_sngCALR,&&op_iBIOSHLE,
		&&op_sngLDB,&&op_sngLDB,&&op_sngLDB,&&op_sngLDB,
		&&op_sngLDB,&&op_sngLDB,&&op_sngLDB,&&op_sngLDB,
		&&op_sngPUSHW,&&op_sngPUSHW,&&op_sngPUSHW,&&op_sngPUSHW,
		&&op_sngPUSHW,&&op_sngPUSHW,&&op_sngPUSHW,&&op_sngPUSHW,
		&&op_sngLDW,&&op_sngLDW,&&op_sngLDW,&&op_sngLDW,
		&&op_sngLDW,&&op_sngLDW,&&op_sngLDW,&&op_sngLDW,
		&&op_sngPUSHL,&&op_sngPUSHL,&&op_sngPUSHL,&&op_sngPUSHL,
		&&op_sngPUSHL,&&op_sngPUSHL,&&op_sngPUSHL,&&op_sngPUSHL,
		&&op_sngLDL,&&op_sngLDL,&&op_sngLDL,&&op_sngLDL,
		&&op_sngLDL,&&op_sngLDL,&&op_sngLDL,&&op_sngLDL,
		&&op_sngPOPW,&&op_sngPOPW,&&op_sngPOPW,&&op_sngPOPW,
		&&op_sngPOPW,&&op_sngPOPW,&&op_sngPOPW,&&op_sngPOPW,
		&&op_nop,&&op_nop,&&op_nop,&&op_nop,&&op_nop,&&op_nop,&&op_nop,&&op_nop,
		&&op_sngPOPL,&&op_sngPOPL,&&op_sngPOPL,&&op_sngPOPL,
		&&op_sngPOPL,&&op_sngPOPL,&&op_sngPOPL,&&op_sngPOPL,
		&&op_sngJR,&&op_sngJR,&&op_sngJR,&&op_sngJR,
		&&op_sngJR,&&op_sngJR,&&op_sngJR,&&op_sngJR,
		&&op_sngJR,&&op_sngJR,&&op_sngJR,&&op_sngJR,
		&&op_sngJR,&&op_sngJR,&&op_sngJR,&&op_sngJR,
		&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,
		&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,
		&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,
		&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,&&op_sngJRL,
		&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,
		&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,
		&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,
		&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,
		&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,
		&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,
		&&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,
		&&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,
		&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_sub_B,&&op_nop,&&op_reg_B,
		&&op_reg_B,&&op_reg_B,&&op_reg_B,&&op_reg_B,&&op_reg_B,&&op_reg_B,&&op_reg_B,&&op_reg_B,
		&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_sub_W,&&op_nop,&&op_reg_W,
		&&op_reg_W,&&op_reg_W,&&op_reg_W,&&op_reg_W,&&op_reg_W,&&op_reg_W,&&op_reg_W,&&op_reg_W,
		&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_sub_L,&&op_nop,&&op_reg_L,
		&&op_reg_L,&&op_reg_L,&&op_reg_L,&&op_reg_L,&&op_reg_L,&&op_reg_L,&&op_reg_L,&&op_reg_L,
		&&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_dst,  &&op_nop,&&op_sngLDX,
		&&op_sngSWI,&&op_sngSWI,&&op_sngSWI,&&op_sngSWI,
		&&op_sngSWI,&&op_sngSWI,&&op_sngSWI,&&op_sngSWI,
	};

next:
	brCode = false;
	first  = loadB(pc++);
	cycles_extra = 0;
	goto *extra_labels[first];

	/* --- Addressing mode labels --- */
xa_nop:  goto do_op;
xa_XWA:  mem = regL(0); goto do_op;
xa_XBC:  mem = regL(1); goto do_op;
xa_XDE:  mem = regL(2); goto do_op;
xa_XHL:  mem = regL(3); goto do_op;
xa_XIX:  mem = regL(4); goto do_op;
xa_XIY:  mem = regL(5); goto do_op;
xa_XIZ:  mem = regL(6); goto do_op;
xa_XSP:  mem = regL(7); goto do_op;
xa_XWAd: mem = regL(0) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XBCd: mem = regL(1) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XDEd: mem = regL(2) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XHLd: mem = regL(3) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XIXd: mem = regL(4) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XIYd: mem = regL(5) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XIZd: mem = regL(6) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;
xa_XSPd: mem = regL(7) + (int8)loadB(pc++); cycles_extra = 2; goto do_op;

xa_8:  mem = loadB(pc++); cycles_extra = 2; goto do_op;
xa_16:
{	uint16 w = loadB(pc+1); w = (w << 8) | loadB(pc);
	pc += 2; mem = w; cycles_extra = 2; goto do_op; }
xa_24:
{	uint16 a = loadB(pc+1); a = (a << 8) | loadB(pc); pc += 2;
	uint32 b = loadB(pc++);
	mem = (b << 16) | a; cycles_extra = 3; goto do_op; }
xa_R32:
{	uint8 d = loadB(pc++);
	if (d == 0x03) {
		uint8 rs = loadB(pc++), ri = loadB(pc++);
		mem = rCodeL(rs) + (int8)rCodeB(ri); cycles_extra = 8;
	} else if (d == 0x07) {
		uint8 rs = loadB(pc++), ri = loadB(pc++);
		mem = rCodeL(rs) + (int16)rCodeW(ri); cycles_extra = 8;
	} else if (d == 0x13) {
		int16 disp = (int16)((loadB(pc+1) << 8) | loadB(pc)); pc += 2;
		mem = pc + disp; cycles_extra = 8;
	} else {
		cycles_extra = 5; mem = rCodeL(d);
		if ((d & 3) == 1) {
			uint16 hi = loadB(pc+1), lo = loadB(pc);
			pc += 2; mem += (int16)((hi << 8) | lo);
		}
	}
	goto do_op; }
xa_Dec:
{	uint8 d = loadB(pc++), r = d & 0xFC;
	cycles_extra = 3;
	switch(d & 3) {
	case 0: rCodeL(r) -= 1; mem = rCodeL(r); break;
	case 1: rCodeL(r) -= 2; mem = rCodeL(r); break;
	case 2: rCodeL(r) -= 4; mem = rCodeL(r); break;
	}
	goto do_op; }
xa_Inc:
{	uint8 d = loadB(pc++), r = d & 0xFC;
	cycles_extra = 3;
	switch(d & 3) {
	case 0: mem = rCodeL(r); rCodeL(r) += 1; break;
	case 1: mem = rCodeL(r); rCodeL(r) += 2; break;
	case 2: mem = rCodeL(r); rCodeL(r) += 4; break;
	}
	goto do_op; }
xa_RC:
	brCode = true; rCode = loadB(pc++); cycles_extra = 1;
	goto do_op;

	/* --- Primary opcode dispatch --- */
do_op:
	goto *op_labels[first];

	/* Sub-decoder shims: fetch second byte, call into srcDecode/dstDecode/regDecode */
op_sub_B:
	second = loadB(pc++); R = second & 7; size = 0;
	srcDecode[second](); goto done;
op_sub_W:
	second = loadB(pc++); R = second & 7; size = 1;
	srcDecode[second](); goto done;
op_sub_L:
	second = loadB(pc++); R = second & 7; size = 2;
	srcDecode[second](); goto done;
op_dst:
	second = loadB(pc++); R = second & 7;
	dstDecode[second](); goto done;
op_reg_B:
{	static const uint8 cv[8]={0xE1,0xE0,0xE5,0xE4,0xE9,0xE8,0xED,0xEC};
	second=loadB(pc++); R=second&7; size=0;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	regDecode[second](); goto done; }
op_reg_W:
{	static const uint8 cv[8]={0xE0,0xE4,0xE8,0xEC,0xF0,0xF4,0xF8,0xFC};
	second=loadB(pc++); R=second&7; size=1;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	regDecode[second](); goto done; }
op_reg_L:
{	static const uint8 cv[8]={0xE0,0xE4,0xE8,0xEC,0xF0,0xF4,0xF8,0xFC};
	second=loadB(pc++); R=second&7; size=2;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	regDecode[second](); goto done; }
op_nop: cycles = 2; goto done;

	/* Single-instruction handler shims — call existing functions */
op_sngNOP:    sngNOP();    goto done;
op_sngNORMAL: sngNORMAL(); goto done;
op_sngPUSHSR: sngPUSHSR(); goto done;
op_sngPOPSR:  sngPOPSR();  goto done;
op_sngMAX:    sngMAX();    goto done;
op_sngHALT:   sngHALT();   goto done;
op_sngEI:     sngEI();     goto done;
op_sngRETI:   sngRETI();   goto done;
op_sngLD8_8:  sngLD8_8();  goto done;
op_sngPUSH8:  sngPUSH8();  goto done;
op_sngLD8_16: sngLD8_16(); goto done;
op_sngPUSH16: sngPUSH16(); goto done;
op_sngINCF:   sngINCF();   goto done;
op_sngDECF:   sngDECF();   goto done;
op_sngRET:    sngRET();    goto done;
op_sngRETD:   sngRETD();   goto done;
op_sngRCF:    sngRCF();    goto done;
op_sngSCF:    sngSCF();    goto done;
op_sngCCF:    sngCCF();    goto done;
op_sngZCF:    sngZCF();    goto done;
op_sngPUSHA:  sngPUSHA();  goto done;
op_sngPOPA:   sngPOPA();   goto done;
op_sngEX:     sngEX();     goto done;
op_sngLDF:    sngLDF();    goto done;
op_sngPUSHF:  sngPUSHF();  goto done;
op_sngPOPF:   sngPOPF();   goto done;
op_sngJP16:   sngJP16();   goto done;
op_sngJP24:   sngJP24();   goto done;
op_sngCALL16: sngCALL16(); goto done;
op_sngCALL24: sngCALL24(); goto done;
op_sngCALR:   sngCALR();   goto done;
op_sngLDB:    sngLDB();    goto done;
op_sngPUSHW:  sngPUSHW();  goto done;
op_sngLDW:    sngLDW();    goto done;
op_sngPUSHL:  sngPUSHL();  goto done;
op_sngLDL:    sngLDL();    goto done;
op_sngPOPW:   sngPOPW();   goto done;
op_sngPOPL:   sngPOPL();   goto done;
op_sngJR:     sngJR();     goto done;
op_sngJRL:    sngJRL();    goto done;
op_sngLDX:    sngLDX();    goto done;
op_sngSWI:    sngSWI();    goto done;
op_iBIOSHLE:  iBIOSHLE();  goto done;

done:
	return cycles + cycles_extra;
}
