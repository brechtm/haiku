/*
 * Copyright 2002-2005, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_ARCH_CPU_H
#define _KERNEL_ARCH_CPU_H


#include <OS.h>
#include <boot/kernel_args.h>


#define PAGE_ALIGN(x) (((x) + (B_PAGE_SIZE - 1)) & ~(B_PAGE_SIZE - 1))

#ifdef __cplusplus
extern "C" {
#endif 

status_t arch_cpu_preboot_init(kernel_args *args);
status_t arch_cpu_init(kernel_args *args);
status_t arch_cpu_init_post_vm(kernel_args *args);
status_t arch_cpu_init_post_modules(kernel_args *args);
status_t arch_cpu_shutdown(bool reboot);

void arch_cpu_invalidate_TLB_range(addr_t start, addr_t end);
void arch_cpu_invalidate_TLB_list(addr_t pages[], int num_pages);
void arch_cpu_global_TLB_invalidate(void);

status_t arch_cpu_user_memcpy(void *to, const void *from, size_t size, addr_t *faultHandler);
ssize_t arch_cpu_user_strlcpy(char *to, const char *from, size_t size, addr_t *faultHandler);
status_t arch_cpu_user_memset(void *s, char c, size_t count, addr_t *faultHandler);

void arch_cpu_idle(void);
void arch_cpu_sync_icache(void *address, size_t length);

#ifdef __cplusplus
}
#endif 

#include <arch_cpu.h>

#endif /* _KERNEL_ARCH_CPU_H */
