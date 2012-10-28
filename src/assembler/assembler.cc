#include "src/assembler/assembler.h"

#include <cassert>
#include <iomanip>
#include <set>

#include "src/att/att_writer.h"

using namespace std;
using namespace x64;

namespace {

// This table encodes both the mod/rm and the sib byte tables.
// The two are transposes of each other.
unsigned char hex_table_[4][16][16] {
	{{0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38},
	 {0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39,0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39},
	 {0x02,0x0a,0x12,0x1a,0x22,0x2a,0x32,0x3a,0x02,0x0a,0x12,0x1a,0x22,0x2a,0x32,0x3a},
	 {0x03,0x0b,0x13,0x1b,0x23,0x2b,0x33,0x3b,0x03,0x0b,0x13,0x1b,0x23,0x2b,0x33,0x3b},
	 {0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c,0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c},
	 {0x05,0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d,0x05,0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d},
	 {0x06,0x0e,0x16,0x1e,0x26,0x2e,0x36,0x3e,0x06,0x0e,0x16,0x1e,0x26,0x2e,0x36,0x3e},
	 {0x07,0x0f,0x17,0x1f,0x27,0x2f,0x37,0x3f,0x07,0x0f,0x17,0x1f,0x27,0x2f,0x37,0x3f},
	 {0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38,0x00,0x08,0x10,0x18,0x20,0x28,0x30,0x38},
	 {0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39,0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39},
	 {0x02,0x0a,0x12,0x1a,0x22,0x2a,0x32,0x3a,0x02,0x0a,0x12,0x1a,0x22,0x2a,0x32,0x3a},
	 {0x03,0x0b,0x13,0x1b,0x23,0x2b,0x33,0x3b,0x03,0x0b,0x13,0x1b,0x23,0x2b,0x33,0x3b},
	 {0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c,0x04,0x0c,0x14,0x1c,0x24,0x2c,0x34,0x3c},
	 {0x05,0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d,0x05,0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d},
	 {0x06,0x0e,0x16,0x1e,0x26,0x2e,0x36,0x3e,0x06,0x0e,0x16,0x1e,0x26,0x2e,0x36,0x3e},
	 {0x07,0x0f,0x17,0x1f,0x27,0x2f,0x37,0x3f,0x07,0x0f,0x17,0x1f,0x27,0x2f,0x37,0x3f}},

	{{0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78},
	 {0x41,0x49,0x51,0x59,0x61,0x69,0x71,0x79,0x41,0x49,0x51,0x59,0x61,0x69,0x71,0x79},
	 {0x42,0x4a,0x52,0x5a,0x62,0x6a,0x72,0x7a,0x42,0x4a,0x52,0x5a,0x62,0x6a,0x72,0x7a},
	 {0x43,0x4b,0x53,0x5b,0x63,0x6b,0x73,0x7b,0x43,0x4b,0x53,0x5b,0x63,0x6b,0x73,0x7b},
	 {0x44,0x4c,0x54,0x5c,0x64,0x6c,0x74,0x7c,0x44,0x4c,0x54,0x5c,0x64,0x6c,0x74,0x7c},
	 {0x45,0x4d,0x55,0x5d,0x65,0x6d,0x75,0x7d,0x45,0x4d,0x55,0x5d,0x65,0x6d,0x75,0x7d},
	 {0x46,0x4e,0x56,0x5e,0x66,0x6e,0x76,0x7e,0x46,0x4e,0x56,0x5e,0x66,0x6e,0x76,0x7e},
	 {0x47,0x4f,0x57,0x5f,0x67,0x6f,0x77,0x7f,0x47,0x4f,0x57,0x5f,0x67,0x6f,0x77,0x7f},
	 {0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78},
	 {0x41,0x49,0x51,0x59,0x61,0x69,0x71,0x79,0x41,0x49,0x51,0x59,0x61,0x69,0x71,0x79},
	 {0x42,0x4a,0x52,0x5a,0x62,0x6a,0x72,0x7a,0x42,0x4a,0x52,0x5a,0x62,0x6a,0x72,0x7a},
	 {0x43,0x4b,0x53,0x5b,0x63,0x6b,0x73,0x7b,0x43,0x4b,0x53,0x5b,0x63,0x6b,0x73,0x7b},
	 {0x44,0x4c,0x54,0x5c,0x64,0x6c,0x74,0x7c,0x44,0x4c,0x54,0x5c,0x64,0x6c,0x74,0x7c},
	 {0x45,0x4d,0x55,0x5d,0x65,0x6d,0x75,0x7d,0x45,0x4d,0x55,0x5d,0x65,0x6d,0x75,0x7d},
	 {0x46,0x4e,0x56,0x5e,0x66,0x6e,0x76,0x7e,0x46,0x4e,0x56,0x5e,0x66,0x6e,0x76,0x7e},
	 {0x47,0x4f,0x57,0x5f,0x67,0x6f,0x77,0x7f,0x47,0x4f,0x57,0x5f,0x67,0x6f,0x77,0x7f}},

	{{0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8,0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8},
	 {0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9,0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9},
	 {0x82,0x8a,0x92,0x9a,0xa2,0xaa,0xb2,0xba,0x82,0x8a,0x92,0x9a,0xa2,0xaa,0xb2,0xba},
	 {0x83,0x8b,0x93,0x9b,0xa3,0xab,0xb3,0xbb,0x83,0x8b,0x93,0x9b,0xa3,0xab,0xb3,0xbb},
	 {0x84,0x8c,0x94,0x9c,0xa4,0xac,0xb4,0xbc,0x84,0x8c,0x94,0x9c,0xa4,0xac,0xb4,0xbc},
	 {0x85,0x8d,0x95,0x9d,0xa5,0xad,0xb5,0xbd,0x85,0x8d,0x95,0x9d,0xa5,0xad,0xb5,0xbd},
	 {0x86,0x8e,0x96,0x9e,0xa6,0xae,0xb6,0xbe,0x86,0x8e,0x96,0x9e,0xa6,0xae,0xb6,0xbe},
	 {0x87,0x8f,0x97,0x9f,0xa7,0xaf,0xb7,0xbf,0x87,0x8f,0x97,0x9f,0xa7,0xaf,0xb7,0xbf},
	 {0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8,0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8},
	 {0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9,0x81,0x89,0x91,0x99,0xa1,0xa9,0xb1,0xb9},
	 {0x82,0x8a,0x92,0x9a,0xa2,0xaa,0xb2,0xba,0x82,0x8a,0x92,0x9a,0xa2,0xaa,0xb2,0xba},
	 {0x83,0x8b,0x93,0x9b,0xa3,0xab,0xb3,0xbb,0x83,0x8b,0x93,0x9b,0xa3,0xab,0xb3,0xbb},
	 {0x84,0x8c,0x94,0x9c,0xa4,0xac,0xb4,0xbc,0x84,0x8c,0x94,0x9c,0xa4,0xac,0xb4,0xbc},
	 {0x85,0x8d,0x95,0x9d,0xa5,0xad,0xb5,0xbd,0x85,0x8d,0x95,0x9d,0xa5,0xad,0xb5,0xbd},
	 {0x86,0x8e,0x96,0x9e,0xa6,0xae,0xb6,0xbe,0x86,0x8e,0x96,0x9e,0xa6,0xae,0xb6,0xbe},
	 {0x87,0x8f,0x97,0x9f,0xa7,0xaf,0xb7,0xbf,0x87,0x8f,0x97,0x9f,0xa7,0xaf,0xb7,0xbf}},

	{{0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8,0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8},
	 {0xc1,0xc9,0xd1,0xd9,0xe1,0xe9,0xf1,0xf9,0xc1,0xc9,0xd1,0xd9,0xe1,0xe9,0xf1,0xf9},
	 {0xc2,0xca,0xd2,0xda,0xe2,0xea,0xf2,0xfa,0xc2,0xca,0xd2,0xda,0xe2,0xea,0xf2,0xfa},
	 {0xc3,0xcb,0xd3,0xdb,0xe3,0xeb,0xf3,0xfb,0xc3,0xcb,0xd3,0xdb,0xe3,0xeb,0xf3,0xfb},
	 {0xc4,0xcc,0xd4,0xdc,0xe4,0xec,0xf4,0xfc,0xc4,0xcc,0xd4,0xdc,0xe4,0xec,0xf4,0xfc},
	 {0xc5,0xcd,0xd5,0xdd,0xe5,0xed,0xf5,0xfd,0xc5,0xcd,0xd5,0xdd,0xe5,0xed,0xf5,0xfd},
	 {0xc6,0xce,0xd6,0xde,0xe6,0xee,0xf6,0xfe,0xc6,0xce,0xd6,0xde,0xe6,0xee,0xf6,0xfe},
	 {0xc7,0xcf,0xd7,0xdf,0xe7,0xef,0xf7,0xff,0xc7,0xcf,0xd7,0xdf,0xe7,0xef,0xf7,0xff},
	 {0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8,0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8},
	 {0xc1,0xc9,0xd1,0xd9,0xe1,0xe9,0xf1,0xf9,0xc1,0xc9,0xd1,0xd9,0xe1,0xe9,0xf1,0xf9},
	 {0xc2,0xca,0xd2,0xda,0xe2,0xea,0xf2,0xfa,0xc2,0xca,0xd2,0xda,0xe2,0xea,0xf2,0xfa},
	 {0xc3,0xcb,0xd3,0xdb,0xe3,0xeb,0xf3,0xfb,0xc3,0xcb,0xd3,0xdb,0xe3,0xeb,0xf3,0xfb},
	 {0xc4,0xcc,0xd4,0xdc,0xe4,0xec,0xf4,0xfc,0xc4,0xcc,0xd4,0xdc,0xe4,0xec,0xf4,0xfc},
	 {0xc5,0xcd,0xd5,0xdd,0xe5,0xed,0xf5,0xfd,0xc5,0xcd,0xd5,0xdd,0xe5,0xed,0xf5,0xfd},
	 {0xc6,0xce,0xd6,0xde,0xe6,0xee,0xf6,0xfe,0xc6,0xce,0xd6,0xde,0xe6,0xee,0xf6,0xfe},
	 {0xc7,0xcf,0xd7,0xdf,0xe7,0xef,0xf7,0xff,0xc7,0xcf,0xd7,0xdf,0xe7,0xef,0xf7,0xff}}};

// This is a direct index into the hex_table
inline unsigned char mod_rm(size_t mod, size_t rm, size_t r) {
	assert(mod < 4 && rm < 16 && r < 16);
	return hex_table_[mod][rm][r];
}

// This is a transposed index into the table
inline unsigned char sib(size_t scale, size_t index, size_t base) {
	assert(scale < 4 && index < 16 && base < 16);
	return hex_table_[scale][base][index];
}

inline void emit(unsigned char*& buf, unsigned char c) {
	(*buf++) = c;
}

inline void emit_mem_prefix(unsigned char*& buf, M m) {
	if ( m.get_size_or() )
		emit(buf, 0x67);
}

inline void emit_prefix(unsigned char*& buf, unsigned char c) {
	emit(buf, c);
}

inline void emit_prefix(unsigned char*& buf, unsigned char c1,
		                    unsigned char c2) {
	emit(buf, c1);
	emit(buf, c2);
}

inline void emit_prefix(unsigned char*& buf, unsigned char c1,
		                    unsigned char c2, unsigned char c3) {
	emit(buf, c1);
	emit(buf, c2);
	emit(buf, c3);
}

inline void emit_opcode(unsigned char*& buf, unsigned char c) {
	emit(buf, c);
}

inline void emit_opcode(unsigned char*& buf, unsigned char c, Operand delta) {
	emit(buf, c + (0x7 & delta));
}

inline void emit_opcode(unsigned char*& buf, unsigned char c1, 
		                    unsigned char c2) {
	emit(buf, c1);
	emit(buf, c2);
}

inline void emit_opcode(unsigned char*& buf, unsigned char c1, 
		                    unsigned char c2, Operand delta) {
	emit(buf, c1);
	emit(buf, c2 + (0x7 & delta));
}

inline void emit_opcode(unsigned char*& buf, unsigned char c1, 
												unsigned char c2, unsigned char c3) {
	emit(buf, c1);
	emit(buf, c2);
	emit(buf, c3);
}

inline void emit_opcode(unsigned char*& buf, unsigned char c1, 
		                    unsigned char c2, unsigned char c3, Operand delta) {
	emit(buf, c1);
	emit(buf, c2);
	emit(buf, c3 + (0x7 & delta));
}

inline void emit_imm(unsigned char*& buf, Imm8 imm) {
	emit(buf, imm & 0xff); 
}

inline void emit_imm(unsigned char*& buf, Imm16 imm) {
	*((uint16_t*) buf) = imm;
	buf += 2;
}

inline void emit_imm(unsigned char*& buf, Imm32 imm) {
	*((uint32_t*) buf) = imm;
	buf += 4;
}

inline void emit_imm(unsigned char*& buf, Imm64 imm) {
	*((uint64_t*) buf) = imm;
	buf += 8;
}

// MOD R/M nop -- Simplifies codegen.
// This corresponds to instructions without any explicit operands.
// Calls to this method should be inlined away.
inline void emit_mod_rm(unsigned char*& buf) {
}

// MOD R/M nop -- Simplifies codegen.
// This corresponds to instructions with a single operand and no digit bit.
// This is the class of instructions that encode operand directly in opcode.
// See bswap for example.
// Calls to this method should be inlined away.
inline void emit_mod_rm(unsigned char*& buf, Operand ignore) {
}

// This ignores the distinction between high and low general purpose regs,
//   It won't work correctly for AH, BH, CH, DH
inline void emit_mod_rm(unsigned char*& buf, Operand rm, Operand r) {
	assert(!((R64)rm).is_null());
	assert(!((R64)r).is_null());
	emit(buf, mod_rm(3, rm, r));
}

// This ignores the distinction between high and low general purpose regs,
//   It won't work correctly for AH, BH, CH, DH
inline void emit_mod_rm(unsigned char*& buf, M rm, Operand r) {
	assert(!((R64)r).is_null());
	assert(!((M)rm).is_null());

	auto b = rm.get_base();
	auto i = rm.get_index();
	const auto disp = (int32_t) rm.get_disp();

	const auto base_null = b.is_null();
	const auto index_null = i.is_null();

	const auto disp0 = disp == 0;
	const auto disp8 = disp >= -128 && disp < 128;
	const auto disp32 = disp < -128 || disp >= 128;
	const auto rip_disp32 = (b & 0xfffffffffffffff7) == 0x5;

	size_t mod = 0;
	if ( base_null )
		mod = 0;
	else if ( disp0 )
		mod = rip_disp32 ? 1 : 0;
	else if ( disp8 )
		mod = 1;
	else
		mod = 2;

	// Emit both mod/rm and sib
	if ( base_null || !index_null || ((b & 0x7) == 0x4) ) {
		emit(buf, mod_rm(mod, 0x4, r));
		emit(buf, sib(rm.get_scale(), index_null ? rsp : i, base_null ? rbp : b));
	}
	// Only emit mod/rm
	else
		emit(buf, mod_rm(mod, b, r));

	// Emit displacement
	if ( disp32 || base_null )
		emit_imm(buf, (Imm32) disp);
	else if ( disp8 && !disp0 )
		emit_imm(buf, (Imm8) disp);
	else if ( rip_disp32 )
		emit_imm(buf, (Imm8) 0);
}

// REX nop -- Simplifies codegen.
// Calls to this method should be inlined away.
inline void emit_rex(unsigned char*& buf, R low) {
}

// REX nop -- Simplifies codegen.
// Calls to this method should be inlined away.
inline void emit_rex(unsigned char*& buf, unsigned char rex, R low) {
}

// Figure 2.7: Intel Manual Vol 2A 2-9
// This ignores the distinction between high and low general purpose regs,
//   but that's fine because it wouldn't get you an rex.b either way.
inline void emit_rex(unsigned char*& buf, Operand rm, unsigned char rex, 
		                 R low) {
	assert(!((R64)rm).is_null());
	rex |= (rm >> 3);

	if ( rex || (low >> 2 == 0x1) )
		emit(buf, rex | 0x40);
}

// Figure 2.5: Intel Manual Vol 2A 2-8
// This ignores the distinction between high and low general purpose regs,
//   but that's fine because it wouldn't get you an rex.b either way.
inline void emit_rex(unsigned char*& buf, Operand rm, Operand r, 
		                 unsigned char rex, R low) {
	assert(!((R64)rm).is_null());
	assert(!((R64)r).is_null());

	rex |= (rm >> 3);
	rex |= (r >> 1) & 0x4;

	if ( rex || (low >> 2 == 0x1) )
		emit(buf, rex | 0x40);
}

// Figure 2.4 & 2.6: Intel Manual Vol 2A 2-8 & 2.9
// This ignores the distinction between high and low general purpose regs,
//   but that's fine because it wouldn't get you an rex.b either way.
inline void emit_rex(unsigned char*& buf, M rm, Operand r, unsigned char rex, 
		                 R low) {
	assert(!((R64)r).is_null());

	rex |= (r >> 1) & 0x4;
	// base is stored in [44:40]
	rex |= (rm >> (40+3)) & 0x1;
	// index is stored in [39:35]
	rex |= (rm >> (35+2)) & 0x2;

	if ( rex || (low >> 2 == 0x1) )
		emit(buf, rex | 0x40);
}

// This is essentially identical to the case above.
// The only difference being that there is no possibility of setting rex.r.
inline void emit_rex(unsigned char*& buf, M rm, 
		                 unsigned char rex, R low) {
	// base is stored in [44:40]
	rex |= (rm >> (40+3)) & 0x1;
	// index is stored in [39:35]
	rex |= (rm >> (35+2)) & 0x2;

	// No check for is8bit since this only takes a mem argument
	assert(low.is_null());
	if ( rex )
		emit(buf, rex | 0x40);
}

} // namespace

namespace x64 {

void Assembler::start(Function& fxn) {
	start(fxn.buffer_);
}

void Assembler::assemble(const Instruction& i) {
	switch ( i.get_opcode() >> 50 ) {
		case (LABEL_DEFN_64L >> 50):
			bind(i.get_operand(0));
			break;
        
      	// 4000-way switch
		#include "src/gen/assembler.switch"
		
		default:
			assert(false);
			emit(buf_, 0x90);
	}
}

void Assembler::finish() {
	for ( const auto& jump : jumps_ ) {
		auto pos = jump.first;
		const auto itr = labels_.find(jump.second);
		
		if ( itr == labels_.end() ) 
			emit_imm(pos, Imm32(0));
		else
			emit_imm(pos, (Imm32)(itr->second-pos-4));
	}
}

// void Assembler::adcb(Imm arg0) { } ...
#include "src/gen/assembler.defn"

void Assembler::write_binary(ostream& os, const Code& code) {
	static unsigned char buffer[1024*1024];
	start(buffer);

	for ( const auto& instr : code )
		assemble(instr);
	finish();

	for ( unsigned char* i = buf_begin_; i < buf_; ++i )
		os << *i;
}

void Assembler::write_hex(ostream& os, const Code& code) {
	static unsigned char buffer[1024*1024];
	start(buffer);

	set<unsigned char*> line_breaks;
	for ( const auto& instr : code ) {
		assemble(instr);
		line_breaks.insert(buf_);
	}
	finish();

	for ( unsigned char* i = buf_begin_; i < buf_; ++i ) {
		if ( line_breaks.find(i) != line_breaks.end() )
			os << endl;
		os << hex << setfill('0') << setw(2) << (int) *i << " ";
	}
}

void Assembler::start(unsigned char* buffer) {
	labels_.clear();
	jumps_.clear();

	buf_ = buf_begin_ = buffer;
}

} // namespace x64
