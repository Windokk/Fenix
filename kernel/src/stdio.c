#include "stdio.h"

#include <stdarg.h>
#include "common.h"

void printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

char toupper(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 32; // Convert lowercase letters to uppercase
    }
    return c;
}

void to_hex(unsigned int value, char* buffer, bool uppercase) {
    const char* hex_digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    // Handle 0 explicitly, because the loop won't run for value = 0
    if (value == 0) {
        buffer[i++] = '0';
    }

    // Convert the integer to hex
    while (value > 0) {
        buffer[i++] = hex_digits[value & 0xF];
        value >>= 4;
    }

    // Reverse the buffer to get the correct hex representation
    for (int j = 0; j < i / 2; ++j) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    buffer[i] = '\0'; // Null-terminate the string
}

void ftoa(float value, char* buffer, int precision) {
    int int_part = (int)value;
    float frac_part = value - int_part;

    // Convert integer part to string
    itoa(int_part, buffer, 10);
    
    // If precision > 0, handle fractional part
    if (precision > 0) {
        // Add decimal point
        char* ptr = buffer + strlen(buffer); // Go to end of the string
        *ptr++ = '.';

        // Multiply the fractional part by 10^precision
        for (int i = 0; i < precision; i++) {
            frac_part *= 10;
            int frac_digit = (int)frac_part;
            *ptr++ = '0' + frac_digit; // Convert to character
            frac_part -= frac_digit; // Remove the integer part
        }
        *ptr = '\0'; // Null-terminate the string
    }
}

void vprintf(const char* format, va_list args) {
    uint8_t pad = 0;
    char padchar = ' ';
    char buffer[64]; // Temporary buffer for numbers

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == '%') {
                write_serial('%');
                format++;
            } else if (*format == 'c') {
                write_serial((char)va_arg(args, int));
                format++;
            } else if (*format == 's') {
                serial_print((char*)va_arg(args, char*));
                format++;
            } else if (*format == 'd' || *format == 'i') {
                int value = va_arg(args, int);
                if (value < 0) {
                    write_serial('-');
                    value = -value;
                }
                itoa(value, buffer, 10);
                serial_print(buffer);
                format++;
            } else if (*format == 'u') {
                unsigned int value = va_arg(args, unsigned int);
                itoa(value, buffer, 10);
                serial_print(buffer);
                format++;
            } else if (*format == 'x') {
                unsigned int value = va_arg(args, unsigned int);
                to_hex(value, buffer, false);
                serial_print(buffer);
                format++;
            } else if (*format == 'X') {
                unsigned int value = va_arg(args, unsigned int);
                to_hex(value, buffer, true);
                serial_print(buffer);
                format++;
            } else if (*format == 'f') {  // Handle float values
                float value = va_arg(args, double); // va_arg promotes float to double
                int precision = 6;  // Default precision, you can adjust it
                ftoa(value, buffer, precision); // Convert float to string
                serial_print(buffer);
                format++;
            } else if (*format >= '0' && *format <= '9') {
                if (*format == '0') {
                    padchar = '0';
                    format++;
                }
                while (*format >= '0' && *format <= '9') {
                    pad = pad * 10 + (*format++ - '0');
                }
            } else {
                write_serial('%');
                write_serial(*format++);
            }
        } else {
            write_serial(*format++);
        }
    }
}
