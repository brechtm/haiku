/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include <string.h>
#include <stdio.h>
#include <syscalls.h>


int
printf(const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	_kern_write(2, 0, buf, strlen(buf));

	return i;
}
