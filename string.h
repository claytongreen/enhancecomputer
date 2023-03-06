#pragma once

struct string_t {
	uint8_t* data;
	size_t length;
};

struct string_list_node_t {
	string_list_node_t* next;
	string_t string;
};

struct string_list_t {
	string_list_node_t* first;
	string_list_node_t* last;
	size_t node_count;
	size_t total_length;
};

#define STRING_FMT(s) (int)((s).length), (s).data

static string_t string_create(uint8_t* data, size_t length) {
	string_t* string = PUSH_ARRAY(string_t, 1);
	string->data = data;
	string->length = length;
	return *string;
}

static string_t string_cstring(const char* s) {
	string_t string = string_create((uint8_t*)s, strlen(s));
	return string;
}

#define STRING_LIT(s) string((uint8_t *)(s), sizeof(s) - 1)

static void string_list_push(string_list_t* list, string_t string) {
	string_list_node_t* node = PUSH_ARRAY(string_list_node_t, 1);
	node->string = string;

	if (list->last) {
		list->last->next = node;
		list->last = node;
	}
	else {
		list->first = node;
		list->last = node;
	}

	list->node_count += 1;
	list->total_length += string.length;
}

static string_t string_list_join(string_list_t* list) {
	// TODO: handle putting things in between list parts
	size_t length = list->total_length;
	
	string_t result = {};
	result.data = PUSH_ARRAY(uint8_t, length);
	result.length = length;

	uint8_t* at = result.data;
	for (string_list_node_t *node = list->first; node != 0; node = node->next) {
		string_t part = node->string;
		memcpy(at, part.data, part.length);
		at += part.length;
	}

	return result;
}

static string_t string_pushfv(const char* fmt, va_list args) {
	// in case we need to try a second time
	va_list args2;
	va_copy(args2, args);

	// try to build the string in 1024 bytes
	size_t buffer_size = 1024;
	uint8_t* buffer = PUSH_ARRAY(uint8_t, buffer_size);
	size_t actual_size = vsnprintf((char*)buffer, buffer_size, fmt, args);

	string_t result = {};
	if (actual_size < buffer_size) {
		global_memory -= (buffer_size - actual_size - 1);
		result = string_create(buffer, actual_size);
	}
	else {
		global_memory -= buffer_size;
		uint8_t* actual_size_buffer = PUSH_ARRAY(uint8_t, actual_size + 1);
		size_t final_size = vsnprintf((char*)actual_size_buffer, actual_size + 1, fmt, args2);
		result = string_create(actual_size_buffer, final_size);
	}
	va_end(args2);

	return result;
}

static string_t string_pushf(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
	string_t result = string_pushfv(fmt, args);
	
	va_end(args);

	return result;
}

static void string_list_pushf(string_list_t* list, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
	string_t string = string_pushfv(fmt, args);
	
	va_end(args);

	string_list_push(list, string);
}

// ----------------------------------------------------------------------------

// TODO: use "arena" and string api
static char bits[16];
static char* bit_string_u8(uint8_t byte) {
	for (size_t i = 0; i < 8; i += 1) {
		bits[i] = (byte >> (7 - i)) & 1 ? '1' : '0';
	}
	bits[8] = 0;
	return bits;
}
static char* bit_string_u16(uint16_t byte) {
	for (size_t i = 0; i < 16; i += 1) {
		bits[i] = (byte >> (15 - i)) & 1 ? '1' : '0';
	}
	return bits;
}