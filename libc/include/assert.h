#ifndef __ASSERT_H
#define __ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression)\
	do {\
		if (!(expression)) {\
			printf("%s:%d: Assertion `%s` failed.\n", __func__, __LINE__, #expression);\
			exit(1);\
		}\
	} while (0)
#endif

#endif // ASSERT_H