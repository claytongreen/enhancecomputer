#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
//#include <malloc.h>
#include <intrin.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

// MEMORY ---------------------------------------------------------------------
static size_t global_memory_size = 4096;
static uint8_t* global_memory_base;
static uint8_t* global_memory;
#define PUSH_ARRAY(TYPE, COUNT) (TYPE *)global_memory; global_memory += sizeof(TYPE) * (COUNT); assert(((size_t)(global_memory - global_memory_base) < global_memory_size) && "OUT OF MEMORY")
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

static char reg_names[2][8][3] = {
	{ "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" },
	{ "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" }
};
// TODO: Is there a better way? Instead of padding it nuls "\0"
static char address_names[8][8] = {
	"bx + si",
	"bx + di",
	"bp + si",
	"bp + di",
	"si\0\0\0\0\0",
	"di\0\0\0\0\0",
	"bp\0\0\0\0\0",
	"bx\0\0\0\0\0",
};

struct disasm_line_t {
	string_t parts[3];
	uint8_t bytes[6];
	size_t byte_count;
};

static uint8_t next_byte(string_t* instruction_stream, disasm_line_t *line) {
	assert(instruction_stream->length > 0 && "Unexpected end of instruction stream");

	uint8_t byte = *(instruction_stream->data);
	instruction_stream->data   += 1;
	instruction_stream->length -= 1;

	line->bytes[line->byte_count++] = byte;

	return byte;
}

static uint16_t next_data(string_t* instruction_stream, disasm_line_t* line, uint8_t w) {
	uint8_t lo = next_byte(instruction_stream, line);

	int16_t data = (int16_t)lo;
	if (w == 1) {
		uint8_t hi = next_byte(instruction_stream, line);
		data |= (((int16_t)hi) << 8);
	}

	return data;
}

static string_t next_address(string_t* instruction_stream, disasm_line_t* line, uint8_t w) {
	string_list_t* sb = PUSH_ARRAY(string_list_t, 1);
	string_list_push(sb, string_cstring("["));

	uint16_t data = next_data(instruction_stream, line, 1);
	string_list_pushf(sb, "%d", data);

	string_list_push(sb, string_cstring("]"));

	string_t address = string_list_join(sb);
	return address;
}

static string_t next_effective_address(string_t *instruction_stream, disasm_line_t *line, uint8_t mod, uint8_t rm) {
	string_list_t* address_builder = PUSH_ARRAY(string_list_t, 1);
	string_list_push(address_builder, string_cstring("["));
	if (mod == 0 && rm == 6) {
		uint16_t data = next_data(instruction_stream, line, 1);
		assert(data != 0 && "Require a direct address in an effective address calculation");

		string_list_pushf(address_builder, "%d", data);
	}
	else {
		string_list_push(address_builder, string_cstring(address_names[rm]));
		if (mod) {
			uint16_t data = next_data(instruction_stream, line, mod == 2 ? 1 : 0);

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
	printf("bits 16");
	printf("\n");

	for (;;) {
		if (instruction_stream.length == 0) break;

		disasm_line_t line = { 0 };

		uint8_t b1 = next_byte(&instruction_stream, &line);

		// TODO:
		// MOV register/memory   to       segment register  1000_1110
		// MOV segment register  to       register/memory   1000_1100
		
		// MOV register/memory   to/from  register          1000_10dw
		if ((b1 >> 2) == 34) {
			line.parts[0] = string_cstring("mov");

			uint8_t d = ((b1 >> 1) & 1);
			uint8_t w = ((b1 >> 0) & 1);

			uint8_t b2 = next_byte(&instruction_stream, &line);
			uint8_t mod = (b2 >> 6);
			uint8_t reg = (b2 >> 3) & 7;
			uint8_t rm  = (b2 >> 0) & 7;

			if (mod == 3) {
				line.parts[1] = string_cstring(reg_names[w][d ? reg : rm]);
				line.parts[2] = string_cstring(reg_names[w][d ? rm : reg]);
			}
			else {
				string_t address  = next_effective_address(&instruction_stream, &line, mod, rm);
				string_t reg_name = string_cstring(reg_names[w][reg]);

				line.parts[1] = d ? reg_name : address;
				line.parts[2] = d ? address : reg_name;
			}
		}
		// MOV immediate         to       register/memory   1100_011w
		else if ((b1 >> 1) == 99) {
			uint8_t w = (b1 & 1);

			uint8_t b2 = next_byte(&instruction_stream, &line);
			uint8_t mod = (b2 >> 6);
			uint8_t rm  = (b2 & 7);

			string_t address = next_effective_address(&instruction_stream, &line, mod, rm);
			uint16_t data    = next_data(&instruction_stream, &line, w);

			line.parts[0] = string_cstring("mov");
			line.parts[1] = address;
			line.parts[2] = string_pushf("%s %d", w ? "word" : "byte", data);
		}
		// MOV immediate         to       register          1011_wREG
		else if ((b1 >> 4) == 11) {
			uint8_t w = ((b1 >> 3) & 1);
			uint8_t reg = b1 & 7;

			int16_t data = next_data(&instruction_stream, &line, w);

			line.parts[0] = string_cstring("mov");
			line.parts[1] = string_cstring(reg_names[w][reg]);
			line.parts[2] = string_pushf("%d", data);
		}
		// MOV memory            to       accumulator       1010_000w
		else if ((b1 >> 1) == 80) {
			uint8_t w = (b1 & 1);

			string_t address = next_address(&instruction_stream, &line, w);

			line.parts[0] = string_cstring("mov");
			// TODO: how do I know if it's al/ah?
			// TODO: should ax/al/ah come from the reg_names instead of hardcoded?
			line.parts[1] = string_cstring(w ? "ax" : "al");
			line.parts[2] = address;
		}
		// MOV accumulator       to       memory            1010_001w
		else if ((b1 >> 1) == 81) {
			uint8_t w = (b1 & 1);

			string_t address = next_address(&instruction_stream, &line, w);

			line.parts[0] = string_cstring("mov");
			// TODO: how do I know if it's al/ah?
			// TODO: should ax/al/ah come from the reg_names instead of hardcoded?
			line.parts[1] = address;
			line.parts[2] = string_cstring(w ? "ax" : "al");
		}
		else {
			fprintf(stderr, "%.*s\n", 8, bit_string_u8(b1));
			assert(false && "Unhandled instruction");
		}

		if (print_bytes) {
			for (size_t i = 0; i < 6; i += 1) {
				if (i < line.byte_count) {
					printf("%02x ", line.bytes[i]);
				}
				else {
					printf("   ");
				}
			}
		}
		printf("%.*s %.*s, %.*s\n", STRING_FMT(line.parts[0]), STRING_FMT(line.parts[1]), STRING_FMT(line.parts[2]));
	}

	return 0;
 }