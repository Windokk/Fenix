#include "string.h"

// inline function to swap two numbers
void swap(char *x, char *y) {
	char t = *x; *x = *y; *y = t;
}

// function to reverse buffer[i..j]
char* reverse(char *buffer, int i, int j) {
	while (i < j)
		swap(&buffer[i++], &buffer[j--]);

	return buffer;
}

char* utoa64(uint64_t value, char* buffer, int base)
{
    int i = 0;

    do {
        uint64_t r = value % base;

        if (r < 10)
            buffer[i++] = '0' + r;
        else
            buffer[i++] = 'A' + (r - 10);

        value /= base;
    } while (value);

    buffer[i] = '\0';

    return reverse(buffer, 0, i - 1);
}

char* itoa64(int64_t value, char* buffer, int base)
{
    if (base == 10 && value < 0)
    {
        *buffer++ = '-';

        uint64_t mag = (uint64_t)(-(value + 1)) + 1;
        utoa64(mag, buffer, base);

        return buffer - 1;
    }

    return utoa64((uint64_t)value, buffer, base);
}

void *memset(void *dest, register int value, register uint64_t len)
{
    uint8_t  byte  = (uint8_t)value;
	uint64_t qword = (uint64_t)byte * 0x0101010101010101ULL;
	uint8_t *p = (uint8_t *)dest;

	while (len > 0 && ((uint64_t)p & 7)) { *p++ = byte; len--; }

	uint64_t *q = (uint64_t *)p;
	while (len >= 8) { *q++ = qword; len -= 8; }

	p = (uint8_t *)q;
	while (len-- > 0) *p++ = byte;

	return dest;
}

void *memcpy(void *dest, void *src, register uint64_t len) {
	uint8_t       *d = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;

	while (len > 0 && ((uint64_t)d & 7)) { *d++ = *s++; len--; }

	uint64_t       *dq = (uint64_t *)d;
	const uint64_t *sq = (const uint64_t *)s;
	while (len >= 8) { *dq++ = *sq++; len -= 8; }

	d = (uint8_t *)dq;
	s = (const uint8_t *)sq;
	while (len-- > 0) *d++ = *s++;

	return dest;
}

void *memmove(void *dest, const void *src, uint64_t len) {
	uint8_t       *d = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;

	if (d == s || len == 0) return dest;

	if (d < s) {
		while (len > 0 && ((uint64_t)d & 7)) { *d++ = *s++; len--; }
		uint64_t *dq = (uint64_t *)d; const uint64_t *sq = (const uint64_t *)s;
		while (len >= 8) { *dq++ = *sq++; len -= 8; }
		d = (uint8_t *)dq; s = (const uint8_t *)sq;
		while (len-- > 0) *d++ = *s++;
	} else {
		d += len; s += len;
		while (len > 0 && ((uint64_t)d & 7)) { *--d = *--s; len--; }
		uint64_t *dq = (uint64_t *)d; const uint64_t *sq = (const uint64_t *)s;
		while (len >= 8) { *--dq = *--sq; len -= 8; }
		d = (uint8_t *)dq; s = (const uint8_t *)sq;
		while (len-- > 0) *--d = *--s;
	}

	return dest;
}

void fast_memcpy(void* dst, void* src, size_t s) {
	size_t qwords    = s / 8;
	size_t remainder = s % 8;

	__asm__ volatile (
		"rep movsq"
		: "+D"(dst), "+S"(src), "+c"(qwords)
		:
		: "memory"
	);

	uint8_t       *d  = (uint8_t *)dst;
	const uint8_t *sp = (const uint8_t *)src;
	while (remainder-- > 0) *d++ = *sp++;
}

int memcmp (const void *str1, const void *str2, size_t count) {
  register const unsigned char *s1 = (const unsigned char*)str1;
  register const unsigned char *s2 = (const unsigned char*)str2;

  while (count-- > 0) {
    if (*s1++ != *s2++) {
		  return s1[-1] < s2[-1] ? -1 : 1;
    }
  }
  return 0;
}

size_t strlen(const char *str) {
	size_t s;
	for(s=0; *str!='\0'; str++) {
    	s++;
	}
	return s;
}

char* strcpy(char* dest, const char* src) {
	char *tmp = dest;
	while ((*dest++ = *src++)) ;
	return tmp;
}

char* strncpy(char* dest, const char* src, size_t num) {
	char* ptr = dest;
	if (dest == NULL)
		return NULL;
	while (*src && num--) {
		*dest = *src;
		dest++;
		src++;
	}
	*dest = '\0';

	return ptr;
}

int strcmp(const char* s1, const char* s2) {
  while(*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- > 0) {
        if (*s1 != *s2)
            return *(const unsigned char*)s1 - *(const unsigned char*)s2;
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return 0;
}