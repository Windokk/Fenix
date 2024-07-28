#include "fenix/syscall.h"

#include "stdio.h"

ssize_t syscall(void)
{
	uint32_t eax;
	ASM("int $128");
	ASM("mov %%eax, %0" : "=a" (eax));
	return (ssize_t) eax;
}


ssize_t syscall_write(const FILE *file, const void *buf, size_t count)
{
	ASM("mov %0, %%eax" :: "a" (SYSCALL_WRITE));
	ASM("mov %0, %%edi" :: "g" ((int) file));
	ASM("mov %0, %%esi" :: "g" (buf));
	ASM("mov %0, %%edx" :: "g" (count));

	/* return the amount of bytes written */
	return count;
}

FILE* syscall_open(const char *filename, int flags, int mode)
{
	ASM("mov %0, %%eax" :: "a" (SYSCALL_OPEN));
	ASM("mov %0, %%edi" :: "g" (filename));
	ASM("mov %0, %%esi" :: "g" (flags));
	ASM("mov %0, %%edx" :: "g" (mode));

	ssize_t response = syscall();

	if (response < 0)
		return NULL;

	return (FILE *) response;
}

int syscall_close(const FILE *stream)
{
	ASM("mov %0, %%eax" :: "a" (SYSCALL_CLOSE));
	ASM("mov %0, %%edi" :: "g" ((int) stream));

	return syscall() == 0 ? 0 : EOF;
}

_Noreturn void syscall_exit(int status)
{
	ASM("mov %0, %%eax" :: "a" (SYSCALL_EXIT));
	ASM("mov %0, %%edi" :: "g" (status));
	syscall();

	__builtin_unreachable();
}