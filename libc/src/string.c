#include "string.h"

#include "stddef.h"
#include "stdlib.h"

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

void* memcpy(void* d, const void* s, uint64_t n) {
	uint8_t* cd = d;
	const uint8_t* cs = s;
	while (n--) *cd++ = *cs++;
	return d;
}

void* memmove(void* d, const void* s, uint64_t n) {
	uint8_t* cd = d;
	const uint8_t* cs = s;
	if (s < d) while (n--) cd[n] = cs[n];
	else while (n--) *cd++ = *cs++;
	return d;
}

void* memset(void* d, uint8_t c, uint64_t n) {
	uint8_t* cd = d;
	while (n--) *cd++ = c;
	return d;
}

char* strchr(const char* s, char c) {
	while (*s) if (*s++ == c) return (char*) s - 1;
	return NULL;
}

int strcmp(const char* a, const char* b) {
	for (; *a == *b && *a; a++, b++);
	return *a - *b;
}

char* strcpy(char* d, const char* s) {
	for (uint64_t i = 0; ; i++) {
		d[i] = s[i];
		if (!s[i]) break;
	}
	return d;
}

char* strdup(const char* s) {
	uint64_t n = strlen(s) + 1;
	char* d = malloc(n);
	if (d == NULL) return NULL;
	strncpy(d, s, n);
	return d;
}

uint64_t strlen(const char* s) {
	uint64_t len = 0;
	while (*s++) len++;
	return len;
}

int strncmp(const char* a, const char* b, uint64_t n) {
	for (; *a == *b && --n && *a; a++, b++);
	return *a - *b;
}

char* strncpy(char* d, const char* s, uint64_t n) {
	for (uint64_t i = 0; i < n; i++) {
		d[i] = s[i];
		if (!s[i]) break;
	}
	return d;
}

char* strrchr(const char* s, char c) {
	uint64_t i = strlen(s) - 1;
	while (1) {
		if (s[i] == c) return (char*) &s[i];
		if (i-- == 0) return NULL;
	}
}