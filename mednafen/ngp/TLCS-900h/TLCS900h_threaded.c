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
#include <retro_inline.h>

/* Include handler implementations and secondary dispatch tables */
#include "TLCS900h_interpret_single.c"
#include "TLCS900h_interpret_src.c"
#include "TLCS900h_interpret_dst.c"
#include "TLCS900h_interpret_reg.c"
#include "TLCS900h_interpret.c"

/* Forward declarations for the switch-based secondary dispatchers.
 * Defined below — the interpreter references them in its shim labels. */
static void tlcs_dispatch_src(uint8 second);
static INLINE void tlcs_dispatch_dst(uint8 second);
static INLINE void tlcs_dispatch_reg(uint8 second);

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

	/* Sub-decoder shims: fetch second byte, then call switch-based
	 * secondary dispatch. The switch dispatchers (tlcs_dispatch_*)
	 * are defined below and inlined by GCC at -O2, eliminating the
	 * function pointer indirection of the original srcDecode table. */
op_sub_B:
	second = loadB(pc++); R = second & 7; size = 0;
	tlcs_dispatch_src(second); goto done;
op_sub_W:
	second = loadB(pc++); R = second & 7; size = 1;
	tlcs_dispatch_src(second); goto done;
op_sub_L:
	second = loadB(pc++); R = second & 7; size = 2;
	tlcs_dispatch_src(second); goto done;
op_dst:
	second = loadB(pc++); R = second & 7;
	tlcs_dispatch_dst(second); goto done;
op_reg_B:
{	static const uint8 cv[8]={0xE1,0xE0,0xE5,0xE4,0xE9,0xE8,0xED,0xEC};
	second=loadB(pc++); R=second&7; size=0;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	tlcs_dispatch_reg(second); goto done; }
op_reg_W:
{	static const uint8 cv[8]={0xE0,0xE4,0xE8,0xEC,0xF0,0xF4,0xF8,0xFC};
	second=loadB(pc++); R=second&7; size=1;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	tlcs_dispatch_reg(second); goto done; }
op_reg_L:
{	static const uint8 cv[8]={0xE0,0xE4,0xE8,0xEC,0xF0,0xF4,0xF8,0xFC};
	second=loadB(pc++); R=second&7; size=2;
	if(!brCode){brCode=true;rCode=cv[first&7];}
	tlcs_dispatch_reg(second); goto done; }
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

/* Secondary dispatch helpers.
 * These use switch instead of function pointer tables so GCC can
 * generate a jump table. The handler functions are in the same TU
 * (via the #includes above) so they can be inlined at -O2. */
static INLINE void tlcs_dispatch_src(uint8 second)
{
	switch(second) {
	case 0x04: srcPUSH(); break;
	case 0x06: srcRLD();  break;
	case 0x07: srcRRD();  break;
	case 0x10: srcLDI();  break;
	case 0x11: srcLDIR(); break;
	case 0x12: srcLDD();  break;
	case 0x13: srcLDDR(); break;
	case 0x14: srcCPI();  break;
	case 0x15: srcCPIR(); break;
	case 0x16: srcCPD();  break;
	case 0x17: srcCPDR(); break;
	case 0x19: srcLD16m(); break;
	case 0x20: case 0x21: case 0x22: case 0x23:
	case 0x24: case 0x25: case 0x26: case 0x27: srcLD();   break;
	case 0x30: case 0x31: case 0x32: case 0x33:
	case 0x34: case 0x35: case 0x36: case 0x37: srcEX();   break;
	case 0x38: srcADDi(); break;
	case 0x39: srcADCi(); break;
	case 0x3A: srcSUBi(); break;
	case 0x3B: srcSBCi(); break;
	case 0x3C: srcANDi(); break;
	case 0x3D: srcXORi(); break;
	case 0x3E: srcORi();  break;
	case 0x3F: srcCPi();  break;
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47: srcMUL();  break;
	case 0x48: case 0x49: case 0x4A: case 0x4B:
	case 0x4C: case 0x4D: case 0x4E: case 0x4F: srcMULS(); break;
	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57: srcDIV();  break;
	case 0x58: case 0x59: case 0x5A: case 0x5B:
	case 0x5C: case 0x5D: case 0x5E: case 0x5F: srcDIVS(); break;
	case 0x60: case 0x61: case 0x62: case 0x63:
	case 0x64: case 0x65: case 0x66: case 0x67: srcINC();  break;
	case 0x68: case 0x69: case 0x6A: case 0x6B:
	case 0x6C: case 0x6D: case 0x6E: case 0x6F: srcDEC();  break;
	case 0x78: srcRLC();  break;
	case 0x79: srcRRC();  break;
	case 0x7A: srcRL();   break;
	case 0x7B: srcRR();   break;
	case 0x7C: srcSLA();  break;
	case 0x7D: srcSRA();  break;
	case 0x7E: srcSLL();  break;
	case 0x7F: srcSRL();  break;
	case 0x80: case 0x81: case 0x82: case 0x83:
	case 0x84: case 0x85: case 0x86: case 0x87: srcADDRm(); break;
	case 0x88: case 0x89: case 0x8A: case 0x8B:
	case 0x8C: case 0x8D: case 0x8E: case 0x8F: srcADDmR(); break;
	case 0x90: case 0x91: case 0x92: case 0x93:
	case 0x94: case 0x95: case 0x96: case 0x97: srcADCRm(); break;
	case 0x98: case 0x99: case 0x9A: case 0x9B:
	case 0x9C: case 0x9D: case 0x9E: case 0x9F: srcADCmR(); break;
	case 0xA0: case 0xA1: case 0xA2: case 0xA3:
	case 0xA4: case 0xA5: case 0xA6: case 0xA7: srcSUBRm(); break;
	case 0xA8: case 0xA9: case 0xAA: case 0xAB:
	case 0xAC: case 0xAD: case 0xAE: case 0xAF: srcSUBmR(); break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3:
	case 0xB4: case 0xB5: case 0xB6: case 0xB7: srcSBCRm(); break;
	case 0xB8: case 0xB9: case 0xBA: case 0xBB:
	case 0xBC: case 0xBD: case 0xBE: case 0xBF: srcSBCmR(); break;
	case 0xC0: case 0xC1: case 0xC2: case 0xC3:
	case 0xC4: case 0xC5: case 0xC6: case 0xC7: srcANDRm(); break;
	case 0xC8: case 0xC9: case 0xCA: case 0xCB:
	case 0xCC: case 0xCD: case 0xCE: case 0xCF: srcANDmR(); break;
	case 0xD0: case 0xD1: case 0xD2: case 0xD3:
	case 0xD4: case 0xD5: case 0xD6: case 0xD7: srcXORRm(); break;
	case 0xD8: case 0xD9: case 0xDA: case 0xDB:
	case 0xDC: case 0xDD: case 0xDE: case 0xDF: srcXORmR(); break;
	case 0xE0: case 0xE1: case 0xE2: case 0xE3:
	case 0xE4: case 0xE5: case 0xE6: case 0xE7: srcORRm();  break;
	case 0xE8: case 0xE9: case 0xEA: case 0xEB:
	case 0xEC: case 0xED: case 0xEE: case 0xEF: srcORmR();  break;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3:
	case 0xF4: case 0xF5: case 0xF6: case 0xF7: srcCPRm();  break;
	case 0xF8: case 0xF9: case 0xFA: case 0xFB:
	case 0xFC: case 0xFD: case 0xFE: case 0xFF: srcCPmR();  break;
	default: break; /* es = no-op for unimplemented slots */
	}
}

static INLINE void tlcs_dispatch_dst(uint8 second)
{
	switch(second) {
	case 0x00: DST_dstLDBi();   break;
	case 0x02: DST_dstLDWi();   break;
	case 0x04: DST_dstPOPB();   break;
	case 0x06: DST_dstPOPW();   break;
	case 0x14: DST_dstLDBm16(); break;
	case 0x16: DST_dstLDWm16(); break;
	case 0x20: case 0x21: case 0x22: case 0x23:
	case 0x24: case 0x25: case 0x26: case 0x27: DST_dstLDAW(); break;
	case 0x28: DST_dstANDCFA(); break;
	case 0x29: DST_dstORCFA();  break;
	case 0x2A: DST_dstXORCFA(); break;
	case 0x2B: DST_dstLDCFA();  break;
	case 0x2C: DST_dstSTCFA();  break;
	case 0x30: case 0x31: case 0x32: case 0x33:
	case 0x34: case 0x35: case 0x36: case 0x37: DST_dstLDAL(); break;
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47: DST_dstLDBR(); break;
	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57: DST_dstLDWR(); break;
	case 0x60: case 0x61: case 0x62: case 0x63:
	case 0x64: case 0x65: case 0x66: case 0x67: DST_dstLDLR(); break;
	case 0x80: case 0x81: case 0x82: case 0x83:
	case 0x84: case 0x85: case 0x86: case 0x87: DST_dstANDCF(); break;
	case 0x90: case 0x91: case 0x92: case 0x93:
	case 0x94: case 0x95: case 0x96: case 0x97: DST_dstORCF();  break;
	case 0x98: case 0x99: case 0x9A: case 0x9B:
	case 0x9C: case 0x9D: case 0x9E: case 0x9F: DST_dstXORCF(); break;
	case 0xA0: case 0xA1: case 0xA2: case 0xA3:
	case 0xA4: case 0xA5: case 0xA6: case 0xA7: DST_dstLDCF();  break;
	case 0xA8: case 0xA9: case 0xAA: case 0xAB:
	case 0xAC: case 0xAD: case 0xAE: case 0xAF: DST_dstSTCF();  break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3:
	case 0xB4: case 0xB5: case 0xB6: case 0xB7: DST_dstTSET();  break;
	case 0xB8: case 0xB9: case 0xBA: case 0xBB:
	case 0xBC: case 0xBD: case 0xBE: case 0xBF: DST_dstRES();   break;
	case 0xC0: case 0xC1: case 0xC2: case 0xC3:
	case 0xC4: case 0xC5: case 0xC6: case 0xC7: DST_dstSET();   break;
	case 0xC8: case 0xC9: case 0xCA: case 0xCB:
	case 0xCC: case 0xCD: case 0xCE: case 0xCF: DST_dstCHG();   break;
	case 0xD0: case 0xD1: case 0xD2: case 0xD3:
	case 0xD4: case 0xD5: case 0xD6: case 0xD7: DST_dstBIT();   break;
	case 0xD8: case 0xD9: case 0xDA: case 0xDB:
	case 0xDC: case 0xDD: case 0xDE: case 0xDF: DST_dstJP();    break;
	case 0xE0: case 0xE1: case 0xE2: case 0xE3:
	case 0xE4: case 0xE5: case 0xE6: case 0xE7:
	case 0xE8: case 0xE9: case 0xEA: case 0xEB:
	case 0xEC: case 0xED: case 0xEE: case 0xEF: DST_dstCALL();  break;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3:
	case 0xF4: case 0xF5: case 0xF6: case 0xF7:
	case 0xF8: case 0xF9: case 0xFA: case 0xFB:
	case 0xFC: case 0xFD: case 0xFE: case 0xFF: DST_dstRET();   break;
	default: break;
	}
}

static INLINE void tlcs_dispatch_reg(uint8 second)
{
	switch(second) {
	case 0x03: regLDi(); break;
	case 0x04: regPUSH(); break;
	case 0x05: regPOP(); break;
	case 0x06: regCPL(); break;
	case 0x07: regNEG(); break;
	case 0x08: regMULi(); break;
	case 0x09: regMULSi(); break;
	case 0x0A: regDIVi(); break;
	case 0x0B: regDIVSi(); break;
	case 0x0C: regLINK(); break;
	case 0x0D: regUNLK(); break;
	case 0x0E: regBS1F(); break;
	case 0x0F: regBS1B(); break;
	case 0x10: regDAA();  break;
	case 0x12: regEXTZ(); break;
	case 0x13: regEXTS(); break;
	case 0x14: regPAA();  break;
	case 0x16: regMIRR(); break;
	case 0x19: regMULA(); break;
	case 0x1C: regDJNZ(); break;
	case 0x20: regANDCFi(); break;
	case 0x21: regORCFi();  break;
	case 0x22: regXORCFi(); break;
	case 0x23: regLDCFi();  break;
	case 0x24: regSTCFi();  break;
	case 0x28: regANDCFA(); break;
	case 0x29: regORCFA();  break;
	case 0x2A: regXORCFA(); break;
	case 0x2B: regLDCFA();  break;
	case 0x2C: regSTCFA();  break;
	case 0x2E: regLDCcrr(); break;
	case 0x2F: regLDCrcr(); break;
	case 0x30: regRES(); break;
	case 0x31: regSET(); break;
	case 0x32: regCHG(); break;
	case 0x33: regBIT(); break;
	case 0x34: regTSET(); break;
	case 0x38: regMINC1(); break;
	case 0x39: regMINC2(); break;
	case 0x3A: regMINC4(); break;
	case 0x3C: regMDEC1(); break;
	case 0x3D: regMDEC2(); break;
	case 0x3E: regMDEC4(); break;
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47: regMUL();  break;
	case 0x48: case 0x49: case 0x4A: case 0x4B:
	case 0x4C: case 0x4D: case 0x4E: case 0x4F: regMULS(); break;
	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57: regDIV();  break;
	case 0x58: case 0x59: case 0x5A: case 0x5B:
	case 0x5C: case 0x5D: case 0x5E: case 0x5F: regDIVS(); break;
	case 0x60: case 0x61: case 0x62: case 0x63:
	case 0x64: case 0x65: case 0x66: case 0x67: regINC();  break;
	case 0x68: case 0x69: case 0x6A: case 0x6B:
	case 0x6C: case 0x6D: case 0x6E: case 0x6F: regDEC();  break;
	case 0x70: case 0x71: case 0x72: case 0x73:
	case 0x74: case 0x75: case 0x76: case 0x77:
	case 0x78: case 0x79: case 0x7A: case 0x7B:
	case 0x7C: case 0x7D: case 0x7E: case 0x7F: regSCC();  break;
	case 0x80: case 0x81: case 0x82: case 0x83:
	case 0x84: case 0x85: case 0x86: case 0x87: regADD();  break;
	case 0x88: case 0x89: case 0x8A: case 0x8B:
	case 0x8C: case 0x8D: case 0x8E: case 0x8F: regLDRr(); break;
	case 0x90: case 0x91: case 0x92: case 0x93:
	case 0x94: case 0x95: case 0x96: case 0x97: regADC();  break;
	case 0x98: case 0x99: case 0x9A: case 0x9B:
	case 0x9C: case 0x9D: case 0x9E: case 0x9F: regLDrR(); break;
	case 0xA0: case 0xA1: case 0xA2: case 0xA3:
	case 0xA4: case 0xA5: case 0xA6: case 0xA7: regSUB();  break;
	case 0xA8: case 0xA9: case 0xAA: case 0xAB:
	case 0xAC: case 0xAD: case 0xAE: case 0xAF: regLDr3(); break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3:
	case 0xB4: case 0xB5: case 0xB6: case 0xB7: regSBC();  break;
	case 0xB8: case 0xB9: case 0xBA: case 0xBB:
	case 0xBC: case 0xBD: case 0xBE: case 0xBF: regEX();   break;
	case 0xC0: case 0xC1: case 0xC2: case 0xC3:
	case 0xC4: case 0xC5: case 0xC6: case 0xC7: regAND();  break;
	case 0xC8: regADDi(); break;
	case 0xC9: regADCi(); break;
	case 0xCA: regSUBi(); break;
	case 0xCB: regSBCi(); break;
	case 0xCC: regANDi(); break;
	case 0xCD: regXORi(); break;
	case 0xCE: regORi();  break;
	case 0xCF: regCPi();  break;
	case 0xD0: case 0xD1: case 0xD2: case 0xD3:
	case 0xD4: case 0xD5: case 0xD6: case 0xD7: regXOR();  break;
	case 0xD8: case 0xD9: case 0xDA: case 0xDB:
	case 0xDC: case 0xDD: case 0xDE: case 0xDF: regCPr3(); break;
	case 0xE0: case 0xE1: case 0xE2: case 0xE3:
	case 0xE4: case 0xE5: case 0xE6: case 0xE7: regOR();   break;
	case 0xE8: regRLCi(); break;
	case 0xE9: regRRCi(); break;
	case 0xEA: regRLi();  break;
	case 0xEB: regRRi();  break;
	case 0xEC: regSLAi(); break;
	case 0xED: regSRAi(); break;
	case 0xEE: regSLLi(); break;
	case 0xEF: regSRLi(); break;
	case 0xF0: case 0xF1: case 0xF2: case 0xF3:
	case 0xF4: case 0xF5: case 0xF6: case 0xF7: regCP();   break;
	case 0xF8: regRLCA(); break;
	case 0xF9: regRRCA(); break;
	case 0xFA: regRLA();  break;
	case 0xFB: regRRA();  break;
	case 0xFC: regSLAA(); break;
	case 0xFD: regSRAA(); break;
	case 0xFE: regSLLA(); break;
	case 0xFF: regSRLA(); break;
	default: break;
	}
}
