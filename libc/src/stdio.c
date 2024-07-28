#include "stdio.h"

#include "ctype.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "system.h"
#include <limits.h>
#include "stdbool.h"

#ifdef __IS_LIBK
	#include "terminal.h"
#else
	#include "fenix/syscall.h"
#endif

int printf(const char* restrict format, ...)
{
	va_list args;
	va_start(args, format);
	int i = vprintf(format, args);
	va_end(args);
	return i;
}

int putchar(int i)
{
#ifdef __IS_LIBK
        terminal_putc((char) i);
#else
	char c = (char) i;
	ssize_t ret = syscall_write(stdout, &c, 1);

	if (ret == SYSCALL_ERR)
		return EOF;
#endif /* __AULAVIK_LIBK */
        return i;
}

int puts(const char *str)
{
    return printf("%s\n", str);
}

int sprintf(char* __restrict dest, const char* __restrict str, ...)
{
	va_list args;
	va_start(args, str);

	int ret = vsprintf(dest, str, args);

	va_end(args);

	return ret;
}

int vprintf(const char* restrict str, va_list parameters)
{
	void *addr = calloc(256); // TODO how big should this be?
	char *dest = (char *) addr;
	int ret = vsprintf(dest, str, parameters);

#ifdef __IS_LIBK
	terminal_puts(dest);
#else
	syscall_write(stdout, dest, strlen(dest));
#endif /* __AULAVIK_LIBK */

	free(addr);
	return ret;
}

static bool print(char *dest, const char *src, size_t length)
{
	char *ptr = dest + strlen(dest);

	for (size_t i = 0; i < length; i++)
		*ptr++ = *src++;

	*ptr = 0;

	return true;
}

char* convert(uint64_t number, int base, int min_digits)
{
	int digits = 0;
	static char chars[] = "0123456789abcdef";
	static char buff[128];
	char *ptr = &buff[127];

	*ptr = '\0';

	do {
		digits++;
		*--ptr = chars[number % base];
		number /= base;
	} while (number);

	while (digits < min_digits) {
		ptr--;
		*ptr = '0';
		digits++;
	}

	return ptr;
}

/**
 * Writes the formatted output of the format string pointed to by str and the
 * provided arguments into the string pointed to by dest.
 *
 * @param dest A pointer to a sufficiently large string to write into
 * @param str A multibyte character sequence, optionally with codes to indicate
 *      the formatting and insertion of an additional provided argument
 * @param parameters A variable argument list of values to insert/format
 * @return The number of characters written, or, if an error occurs, EOF
 */
int vsprintf(char *dest, const char* restrict str, va_list parameters)
{
	/* Necessary in case dest already points to a string */
	dest[0] = '\0';

	int written = 0;
	int min_digits = 0;

	while (*str) {
		size_t maxleft = INT_MAX - written;

		if (str[0] != '%' || str[1] == '%') {
			if (str[0] == '%')
				str++;

			size_t amount = 1;

			while (str[amount] && str[amount] != '%')
				amount++;

			if (maxleft < amount) {
				// TODO set errno to EOVERFLOW
				return EOF;
			}

			if (!print(dest, str, amount))
				return -1;

			str += amount;
			written += amount;
			continue;
		}

		const char *format_begun_at = str++;
		size_t len;

		if (*str >= '0' && *str <= '9') {
			min_digits *= 10;
			min_digits += (*str) - '0';
			str++;
		}

		switch (*str) {
			case 'c':
				str++;
				char c = (char) va_arg(parameters, int);

				if (!maxleft) {
					// TODO set errno to EOVERFLOW
					return EOF;
				}

				if (!print(dest, &c, sizeof(c)))
					return EOF;

				written++;
				break;
			case 'd':
				str++;
				const int num = va_arg(parameters, int);
				// TODO proper abs();
				char *d_str = convert(num * ((num > 0) - (num < 0)), 10, min_digits);
				min_digits = 0;
				len = strlen(d_str);

				if (maxleft < len) {
					// TODO set errno to EOVERFLOW
					return -1;
				}

				if (num < 0) {
					written++;
					char neg = '-';
					if (!print(dest, &neg, sizeof(neg)))
						return EOF;
				}

				if (!print(dest, d_str, len))
					return EOF;

				written += len;
				break;
			case 's':
				str++;
				const char *s_str = va_arg(parameters, const char*);
				len = strlen(s_str);

				if (maxleft < len) {
					// TODO set errno to EOVERFLOW
					return EOF;
				}

				if (!print(dest, s_str, len))
					return EOF;

				written += len;
				break;
			case 'l': /* todo this should be %lx, but format codes
                                        are only 1 char now */
				str++;
				const uint64_t num_hex_64 = va_arg(parameters, uint64_t);
				char *x_str_64 = convert(num_hex_64, 16, min_digits);
				min_digits = 0;
				len = strlen(x_str_64);

				if (maxleft < len) {
					// TODO set errno to EOVERFLOW
					return EOF;
				}

				if (!print(dest, "0x", sizeof("0x") - 1))
					return EOF;

				if (!print(dest, x_str_64, len))
					return EOF;

				written += len + 2;
				break;
			case 'x':
				str++;
				const uint32_t num_hex = va_arg(parameters, uint32_t);
				char *x_str = convert(num_hex, 16, min_digits);
				min_digits = 0;
				len = strlen(x_str);

				if (maxleft < len) {
					// TODO set errno to EOVERFLOW
					return EOF;
				}

				if (!print(dest, "0x", sizeof("0x") - 1))
					return EOF;

				if (!print(dest, x_str, len))
					return EOF;

				written += len + 2;
				break;
			default:
				str = format_begun_at;
				len = strlen(str);

				if (maxleft < len) {
					// TODO set errno to EOVERFLOW
					return EOF;
				}

				if (!print(dest, str, len))
					return EOF;

				written += len;
				str += len;
		}
	}

	return written;
}