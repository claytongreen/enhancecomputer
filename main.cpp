#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
//#include <malloc.h>
#include <intrin.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define assert(Cond) do { if (!(Cond)) __debugbreak(); } while (0)

// MEMORY ---------------------------------------------------------------------
static size_t global_memory_size = 4096;
static uint8_t* global_memory_base;
static uint8_t* global_memory;
#define PUSH_ARRAY(TYPE, COUNT) (TYPE *)global_memory;                                            \
  memset(global_memory, 0, sizeof(TYPE) * (COUNT));                                               \
  global_memory += sizeof(TYPE) * (COUNT);                                                        \
  assert(((size_t)(global_memory - global_memory_base) < global_memory_size) && "OUT OF MEMORY")
// MEMORY ---------------------------------------------------------------------

#include "string.h"

// OS -------------------------------------------------------------------------
static string_t read_entire_file(char* filename)
{
  string_t result = {};

  FILE* file = fopen(filename, "rb");
  if (file != NULL) {
    size_t bytes_read = fread(global_memory, 1, global_memory_size, file);
    if (bytes_read) {
      result.data = global_memory;
      result.length = bytes_read;
      global_memory += bytes_read;
    }
    fclose(file);
  }

  return result;
}
// OS -------------------------------------------------------------------------

// PROGRAM --------------------------------------------------------------------

string_t operand_add    = STRING_LIT("add");
string_t operand_cmp    = STRING_LIT("cmp");
string_t operand_ja     = STRING_LIT("ja");
string_t operand_jb     = STRING_LIT("jb");
string_t operand_jbe    = STRING_LIT("jbe");
string_t operand_jcxz   = STRING_LIT("jcxz");
string_t operand_je     = STRING_LIT("je");
string_t operand_jg     = STRING_LIT("jg");
string_t operand_jl     = STRING_LIT("jl");
string_t operand_jle    = STRING_LIT("jle");
string_t operand_jnb    = STRING_LIT("jnb");
string_t operand_jnl    = STRING_LIT("jnl");
string_t operand_jno    = STRING_LIT("jno");
string_t operand_jnp    = STRING_LIT("jnp");
string_t operand_jns    = STRING_LIT("jns");
string_t operand_jnz    = STRING_LIT("jnz");
string_t operand_jo     = STRING_LIT("jo");
string_t operand_jp     = STRING_LIT("jp");
string_t operand_js     = STRING_LIT("js");
string_t operand_loop   = STRING_LIT("loop");
string_t operand_loopnz = STRING_LIT("loopnz");
string_t operand_loopz  = STRING_LIT("loopz");
string_t operand_mov    = STRING_LIT("mov");
string_t operand_sub    = STRING_LIT("sub");

string_t reg_al = STRING_LIT("al");
string_t reg_cl = STRING_LIT("cl");
string_t reg_dl = STRING_LIT("dl");
string_t reg_bl = STRING_LIT("bl");
string_t reg_ah = STRING_LIT("ah");
string_t reg_ch = STRING_LIT("ch");
string_t reg_dh = STRING_LIT("dh");
string_t reg_bh = STRING_LIT("bh");

string_t reg_ax = STRING_LIT("ax");
string_t reg_cx = STRING_LIT("cx");
string_t reg_dx = STRING_LIT("dx");
string_t reg_bx = STRING_LIT("bx");
string_t reg_sp = STRING_LIT("sp");
string_t reg_bp = STRING_LIT("bp");
string_t reg_si = STRING_LIT("si");
string_t reg_di = STRING_LIT("di");

// TODO: remove
string_t unknown_str = STRING_LIT("???");     

static string_t reg_names[2][8] = {
  {
    reg_al,
    reg_cl,
    reg_dl,
    reg_bl,
    reg_ah,
    reg_ch,
    reg_dh,
    reg_bh,
  },
  {
    reg_ax,
    reg_cx,
    reg_dx,
    reg_bx,
    reg_sp,
    reg_bp,
    reg_si,
    reg_di,
  }
};

static string_t address_names[8] = {
  STRING_LIT("bx + si"),
  STRING_LIT("bx + di"),
  STRING_LIT("bp + si"),
  STRING_LIT("bp + di"),
  STRING_LIT("si"),
  STRING_LIT("di"),
  STRING_LIT("bp"),
  STRING_LIT("bx"),
};

static uint8_t next_byte(string_t* instruction_stream) {
  assert(instruction_stream->length > 0 && "Unexpected end of instruction stream");

  uint8_t byte = *(instruction_stream->data);
  instruction_stream->data   += 1;
  instruction_stream->length -= 1;

  return byte;
}

static int16_t next_data_s16(string_t *instruction_stream) {
  uint8_t lo = next_byte(instruction_stream);
  uint8_t hi = ((lo & 0x80) ? 0xff : 0);
  int16_t data = (((int16_t)hi) << 8) | (int16_t)lo;
  return data;
}

static int8_t next_data_s8(string_t *instruction_stream) {
  int8_t data = (int8_t)next_byte(instruction_stream);
  return data;
}

static uint16_t next_data(string_t* instruction_stream, uint8_t w) {
  uint8_t lo = next_byte(instruction_stream);
  uint8_t hi = (w == 1) ? next_byte(instruction_stream) : ((lo & 0x80) ? 0xff : 0);

  uint16_t data = (((int16_t)hi) << 8) | (uint16_t)lo;

  return data;
}

static string_t next_address(string_t* instruction_stream, uint8_t w) {
  string_list_t* sb = PUSH_ARRAY(string_list_t, 1);
  string_list_push(sb, string_cstring("["));

  uint16_t data = next_data(instruction_stream, 1);
  string_list_pushf(sb, "%d", data);

  string_list_push(sb, string_cstring("]"));

  string_t address = string_list_join(sb);
  return address;
}

static string_t next_effective_address(string_t *instruction_stream, uint8_t mod, uint8_t rm) {
  string_list_t* address_builder = PUSH_ARRAY(string_list_t, 1);
  string_list_push(address_builder, string_cstring("["));
  if (mod == 0 && rm == 6) {
    uint16_t data = next_data(instruction_stream, 1);
    assert(data != 0 && "Require a direct address in an effective address calculation");

    string_list_pushf(address_builder, "%d", data);
  }
  else {
    string_list_push(address_builder, address_names[rm]);
    if (mod) {
      uint16_t data = next_data(instruction_stream, mod == 2 ? 1 : 0);

      // only print if offset isn't 0
      if (data) {
	string_list_pushf(
	  address_builder, " %s %d",
	  data < 0 ? "-" : "+",
	  data < 0 ? (data * -1) : data
	);
      }
    }
  }
  string_list_push(address_builder, string_cstring("]"));
  string_t address = string_list_join(address_builder);
  return address;
}

static string_t next_register_memory(string_t *instruction_stream, uint8_t w, uint8_t mod, uint8_t rm) {
  string_t result = (mod == 3) ? reg_names[w][rm] : next_effective_address(instruction_stream, mod, rm);
  return result;
}

int main(int argc, char** argv)
{
  if (argc == 1) {
    fprintf(stderr, "Usage: sim[filename]\n");
    return 1;
  }

  global_memory_base = (uint8_t *)malloc(global_memory_size);
  assert(global_memory_base && "Failed to allocate data");
  memset(global_memory_base, 0, global_memory_size);
  global_memory = global_memory_base;

  char* filename = argv[1];
  string_t instruction_stream = read_entire_file(filename);
  assert(instruction_stream.length != 0 && "Failed to read file");

  int print_bytes = argc > 2;

  printf("; %s\n", filename);
  printf("\n");
  if (!print_bytes) {
    printf("bits 16");
    printf("\n");
  }

  string_t operand = { 0 };
  string_t dest = { 0 };
  string_t source = { 0 };

  for (;;) {
    if (instruction_stream.length == 0) break;

    uint8_t* instruction_stream_start = instruction_stream.data;

    uint8_t* global_memory_reset = global_memory;

    uint8_t b1 = next_byte(&instruction_stream);
    switch (b1) {
      case 0x00: // ADD b,f,r/m
      case 0x01: // ADD w,f,r/m
      case 0x02: // ADD b,t,r/m
      case 0x03: // ADD w,t,r/m
      case 0x28: // SUB b,f,r/m
      case 0x29: // SUB w,f,r/m
      case 0x2A: // SUB b,t,r/m
      case 0x2B: // SUB w,t,r/m
      case 0x38: // CMP b,f,r/m
      case 0x39: // CMP w,f,r/m
      case 0x3A: // CMP b,t,r/m
      case 0x3B: // CMP w,t,r/m
      case 0x88: // MOV b,f,r/m
      case 0x89: // MOV w,f,r/m
      case 0x8A: // MOV b,t,r/m
      case 0x8B: // MOV w,t,r/m
      {
	uint8_t d = (b1 >> 1) & 1;
	uint8_t w = (b1 >> 0) & 1;

	uint8_t b2 = next_byte(&instruction_stream);
	uint8_t mod = (b2 >> 6);
	uint8_t reg = (b2 >> 3) & 7;
	uint8_t rm  = (b2 >> 0) & 7;

	string_t register_memory_name = next_register_memory(&instruction_stream, w, mod, rm);

	switch (b1) {
	  case 0x00:
	  case 0x01:
	  case 0x02:
	  case 0x03: operand = operand_add; break;

	  case 0x28:
	  case 0x29:
	  case 0x2A:
	  case 0x2B: operand = operand_sub; break;

	  case 0x38:
	  case 0x39:
	  case 0x3A:
	  case 0x3B: operand = operand_cmp; break;

	  case 0x88:
	  case 0x89:
	  case 0x8A:
	  case 0x8B: operand = operand_mov; break;

	  default: 
	    printf("\n!!! Unexpected operand 0x%x\n", b1);
	    assert(false && "Unexpected operand");
	    break;
	}

	dest    = d ? reg_names[w][reg] : register_memory_name;
	source  = d ? register_memory_name : reg_names[w][reg];
      } break;

      case 0x04: // ADD b,ia
      case 0x05: // ADD w,ia
      case 0x2C: // SUB b,ia
      case 0x2D: // SUB w,ia
      case 0x3C: // CMP b,ia
      case 0x3D: // CMP w,ia
      {
	uint8_t w = (b1 >> 0) & 1;

	uint16_t data = next_data(&instruction_stream, w);

	switch (b1) {
	  case 0x04:
	  case 0x05: operand = operand_add; break;

	  case 0x2C:
	  case 0x2D: operand = operand_sub; break;

	  case 0x3C:
	  case 0x3D: operand = operand_cmp; break;

	  default: 
	  printf("\n!!! Unexpected operand 0x%02x\n", b1);
	  assert(false && "Unexpected operand");
	  break;
	}
	switch (b1) {
	  case 0x04:
	  case 0x2C:
	  case 0x3C: dest = reg_al; break;

	  case 0x05:
	  case 0x2D:
	  case 0x3D: dest = reg_ax; break;

	  default: 
	    printf("\n!!! Unexpected dest 0x%02x\n", b1);
	    assert(false && "Unexpected dest");
	    break;
	}
	source = string_pushf("%d", data);
      } break;

      case 0x70: // JO
      case 0x71: // JNO
      case 0x72: // JB/JNAE
      case 0x73: // JNB/JAE
      case 0x74: // JE/JZ
      case 0x76: // JBE/JNA
      case 0x75: // JNE/JNZ
      case 0x77: // JNBE/JA
      case 0x78: // JS
      case 0x79: // JNS
      case 0x7A: // JP/JPE
      case 0x7B: // JNP/JPO
      case 0x7C: // JL/JNGE
      case 0x7D: // JNL/JGE
      case 0x7E: // JLE/JNG
      case 0x7F: // JNLE/JG
      case 0xE0: // LOOPNZ/LOOPNE
      case 0xE1: // LOOPZ/LOOPE
      case 0xE2: // LOOP
      case 0xE3: // JCXZ
      {
	int8_t inc = next_data_s8(&instruction_stream);

	switch(b1) {
	  case 0x70: operand = operand_jo;     break;
	  case 0x71: operand = operand_jno;    break;
	  case 0x72: operand = operand_jb;     break;
	  case 0x73: operand = operand_jnb;    break;
	  case 0x74: operand = operand_je;     break;
	  case 0x75: operand = operand_jnz;    break;
	  case 0x76: operand = operand_jbe;    break;
	  case 0x77: operand = operand_ja;     break;
	  case 0x78: operand = operand_js;     break;
	  case 0x79: operand = operand_jns;    break;
	  case 0x7A: operand = operand_jp;     break;
	  case 0x7B: operand = operand_jnp;    break;
	  case 0x7C: operand = operand_jl;     break;
	  case 0x7D: operand = operand_jnl;    break;
	  case 0x7E: operand = operand_jle;    break;
	  case 0x7F: operand = operand_jg;     break;
	  case 0xE0: operand = operand_loopnz; break;
	  case 0xE1: operand = operand_loopz;  break;
	  case 0xE2: operand = operand_loop;   break;
	  case 0xE3: operand = operand_jcxz;   break;

	  default:
	    printf("\n!!! Unexpected operand 0x%02x\n", b1);
	    assert(false && "Unexpected operand");
	    break;
	}
	dest = string_pushf("%d", inc); // TODO: get the labels somehow
      } break;

      case 0x80: // Immed b,r/m
      case 0x81: // Immed w,r/m
      case 0x82: // Immed b,r/m
      {
	uint8_t w = (b1 >> 0) & 1;

	uint8_t b2 = next_byte(&instruction_stream);
	uint8_t mod = (b2 >> 6);
	uint8_t op  = (b2 >> 3) & 7;
	uint8_t rm  = (b2 >> 0) & 7;

	string_t register_memory_name = next_register_memory(&instruction_stream, w, mod, rm);
	uint16_t data = next_data(&instruction_stream, w);

	switch (op) {
	  case 0: operand = operand_add; break;
	  case 1: assert(false && "(not used)");
	  case 2: assert(false && "TODO: adc");
	  case 3: assert(false && "TODO: sbb");
	  case 4: assert(false && "(not used)");
	  case 5: operand = operand_sub; break;
	  case 6: assert(false && "(not used)");
	  case 7: operand = operand_cmp; break;
	  default: assert(false);
	}
	dest = register_memory_name;
	// TODO: data size? byte/word
	source = string_pushf("%d", data);
      } break;

      // TODO: merge with above. the only difference is next_data_s16 here vs next_data above
      case 0x83: // Immed is,r/m
      {
	uint8_t w = (b1 >> 0) & 1;

	uint8_t b2 = next_byte(&instruction_stream);
	uint8_t mod = (b2 >> 6);
	uint8_t op  = (b2 >> 3) & 7;
	uint8_t rm  = (b2 >> 0) & 7;

	string_t register_memory_name = next_register_memory(&instruction_stream, w, mod, rm);
	int16_t data = next_data_s16(&instruction_stream);

	// TODO: replace with table?
	switch (op) {
	  case 0: operand = operand_add; break;
	  case 1: assert(false && "(not used)");
	  case 2: assert(false && "TODO: adc");
	  case 3: assert(false && "TODO: sbb");
	  case 4: assert(false && "(not used)");
	  case 5: operand = operand_sub; break;
	  case 6: assert(false && "(not used)");
	  case 7: operand = operand_cmp; break;
	  default: assert(false);
	}
	dest = register_memory_name;
	source = string_pushf("%d", data);
      } break;

      case 0xA0: // MOV m -> AL
      case 0xA1: // MOV m -> AX
      case 0xA2: // MOV AX -> m
      case 0xA3: // MOV AX -> m
      {
	uint8_t d = (b1 >> 1) & 1;
	uint8_t w = (b1 >> 0) & 1;

	string_t address = next_address(&instruction_stream, w);
	string_t reg = w ? reg_ax : reg_al;

	operand = operand_mov;
	dest    = d ? address : reg;
	source  = d ? reg : address;
      } break;

      case 0xB0: // MOV i -> AL
      case 0xB1: // MOV i -> CL
      case 0xB5: // MOV i -> CH
      case 0xB9: // MOV i -> CX
      case 0xBA: // MOV i -> DX
      {
	uint8_t w = b1 >= 0xB8; // TODO: does this "scale" (i.e. work for all values? probably not)

	uint16_t data = next_data(&instruction_stream, w);

	operand = operand_mov;
	switch (b1) {
	  case 0xB0: dest = reg_al; break;
	  case 0xB1: dest = reg_cl; break;
	  case 0xB5: dest = reg_ch; break;
	  case 0xB9: dest = reg_cx; break;
	  case 0xBA: dest = reg_dx; break;

	  default:
	  printf("\n!!! Unexpected dest 0x%x\n", b1);
	  assert(false && "Unexpected dest");
	  break;
	}
	source  = string_pushf("%d", data);
      } break;

      case 0xC6: // MOV b,i,r/m
      case 0xC7: // MOV w,i,r/m
      {
	uint8_t w = (b1 >> 0) & 1;

	uint8_t b2 = next_byte(&instruction_stream);
	uint8_t mod = (b2 >> 6);
	// TODO assert(((b2 >> 3) & 7) == 0); // TODO: what this?
	uint8_t rm  = (b2 & 7);

	string_t address = next_effective_address(&instruction_stream, mod, rm);
	uint16_t data    = next_data(&instruction_stream, w);

	operand = operand_mov;
	dest    = address;
	source  = string_pushf("%s %d", w ? "word" : "byte", data);
      } break;


      default:
	printf("\n!!! Unexpected instruction  %s  0x%x\n", bit_string_u8(b1), b1);
	assert(false && "Unexpected instruction");
	break;
    }

    if (print_bytes) {
      size_t len = instruction_stream.data - instruction_stream_start;
      printf(" %s ", bit_string_u8(*instruction_stream_start));
      for (size_t i = 0; i < 6; i += 1) {
	if (i < len) {
	  printf("%02x ", *(instruction_stream_start + i));
	} else {
	  printf("   ");
	}
      }
    }

    if (source.length) {
      printf("%.*s %.*s, %.*s\n", STRING_FMT(operand), STRING_FMT(dest), STRING_FMT(source));
    } else {
      printf("%.*s %.*s\n", STRING_FMT(operand), STRING_FMT(dest));
    }

    global_memory = global_memory_reset;
  }

  return 0;
}
