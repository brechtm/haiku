/*
 * Copyright 2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_ARCH_X86_X86_VM_TRANSLATION_MAP_H
#define KERNEL_ARCH_X86_X86_VM_TRANSLATION_MAP_H


#include <vm/VMTranslationMap.h>


struct X86VMTranslationMap : VMTranslationMap {
								X86VMTranslationMap();
	virtual						~X86VMTranslationMap();

			status_t			Init(bool kernel);

	inline	vm_translation_map_arch_info* ArchData() const
									{ return fArchData; }
	inline	void*				PhysicalPageDir() const
									{ return fArchData->pgdir_phys; }

	virtual	status_t			InitPostSem();

	virtual	status_t 			Lock();
	virtual	status_t			Unlock();

	virtual	addr_t				MappedSize() const;
	virtual	size_t				MaxPagesNeededToMap(addr_t start,
									addr_t end) const;

	virtual	status_t			Map(addr_t virtualAddress,
									addr_t physicalAddress,
									uint32 attributes);
	virtual	status_t			Unmap(addr_t start, addr_t end);

	virtual	status_t			Query(addr_t virtualAddress,
									addr_t* _physicalAddress,
									uint32* _flags);
	virtual	status_t			QueryInterrupt(addr_t virtualAddress,
									addr_t* _physicalAddress,
									uint32* _flags);

	virtual	status_t			Protect(addr_t base, addr_t top,
									uint32 attributes);
	virtual	status_t			ClearFlags(addr_t virtualAddress,
									uint32 flags);

	virtual	void				Flush();

protected:
			vm_translation_map_arch_info* fArchData;
};


#endif	// KERNEL_ARCH_X86_X86_VM_TRANSLATION_MAP_H
