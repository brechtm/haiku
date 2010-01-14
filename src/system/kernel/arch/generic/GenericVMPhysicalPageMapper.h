/*
 * Copyright 2010, Ingo Weinhold <ingo_weinhold@gmx.de>.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_GENERIC_VM_PHYSICAL_PAGE_MAPPER_CLASS_H
#define _KERNEL_GENERIC_VM_PHYSICAL_PAGE_MAPPER_CLASS_H


#include <vm/VMTranslationMap.h>


struct GenericVMPhysicalPageMapper : VMPhysicalPageMapper {
								GenericVMPhysicalPageMapper();
	virtual						~GenericVMPhysicalPageMapper();

	virtual	status_t			GetPage(addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPage(addr_t virtualAddress,
									void* handle);

	virtual	status_t			GetPageCurrentCPU(
									addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPageCurrentCPU(addr_t virtualAddress,
									void* _handle);

	virtual	status_t			GetPageDebug(addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPageDebug(addr_t virtualAddress,
									void* handle);

	virtual	status_t			MemsetPhysical(addr_t address, int value,
									size_t length);
	virtual	status_t			MemcpyFromPhysical(void* to, addr_t from,
									size_t length, bool user);
	virtual	status_t			MemcpyToPhysical(addr_t to, const void* from,
									size_t length, bool user);
	virtual	void				MemcpyPhysicalPage(addr_t to, addr_t from);
};


#endif	// _KERNEL_GENERIC_VM_PHYSICAL_PAGE_MAPPER_CLASS_H
