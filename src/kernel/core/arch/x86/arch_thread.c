/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel.h>
#include <stage2.h>
#include <debug.h>
#include <vm.h>
#include <memheap.h>
#include <thread.h>
#include <arch/thread.h>
#include <arch_cpu.h>
#include <int.h>
#include <string.h>
#include <Errors.h>
#include <signal.h>


// from arch_interrupts.S
extern void	i386_stack_init( struct farcall *interrupt_stack_offset );


void
i386_push_iframe(struct thread *thread, struct iframe *frame)
{
	ASSERT(thread->arch_info.iframe_ptr < IFRAME_TRACE_DEPTH);
	thread->arch_info.iframes[thread->arch_info.iframe_ptr++] = frame;
}


void
i386_pop_iframe(struct thread *thread)
{
	ASSERT(thread->arch_info.iframe_ptr > 0);
	thread->arch_info.iframe_ptr--;
}


int
arch_team_init_team_struct(struct team *p, bool kernel)
{
	return 0;
}


int
arch_thread_init_thread_struct(struct thread *t)
{
	// set up an initial state (stack & fpu)
	memset(&t->arch_info, 0, sizeof(t->arch_info));

	// let the asm function know the offset to the interrupt stack within struct thread
	// I know no better ( = static) way to tell the asm function the offset
	i386_stack_init( &((struct thread*)0)->arch_info.interrupt_stack );

	return 0;
}


int
arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void), void (*entry_func)(void), void (*exit_func)(void))
{
	unsigned int *kstack = (unsigned int *)t->kernel_stack_base;
	unsigned int kstack_size = KSTACK_SIZE;
	unsigned int *kstack_top = kstack + kstack_size / sizeof(unsigned int);
	int i;

//	dprintf("arch_thread_initialize_kthread_stack: kstack 0x%p, start_func 0x%p, entry_func 0x%p\n",
//		kstack, start_func, entry_func);

	// clear the kernel stack
	memset(kstack, 0, kstack_size);

	// set the final return address to be thread_kthread_exit
	kstack_top--;
	*kstack_top = (unsigned int)exit_func;

	// set the return address to be the start of the first function
	kstack_top--;
	*kstack_top = (unsigned int)start_func;

	// set the return address to be the start of the entry (thread setup) function
	kstack_top--;
	*kstack_top = (unsigned int)entry_func;

	// simulate pushfl
//	kstack_top--;
//	*kstack_top = 0x00; // interrupts still disabled after the switch

	// simulate initial popad
	for(i=0; i<8; i++) {
		kstack_top--;
		*kstack_top = 0;
	}

	// save the stack position
	t->arch_info.current_stack.esp = kstack_top;
	t->arch_info.current_stack.ss = (int *)KERNEL_DATA_SEG;

	return 0;
}


void
arch_thread_switch_kstack_and_call(struct thread *t, addr new_kstack, void (*func)(void *), void *arg)
{
	i386_switch_stack_and_call(new_kstack, func, arg);
}


void
arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	addr new_pgdir;
#if 0
	int i;

	dprintf("arch_thread_context_switch: cpu %d 0x%x -> 0x%x, aspace 0x%x -> 0x%x, old stack = 0x%x:0x%x, stack = 0x%x:0x%x\n",
		smp_get_current_cpu(), t_from->id, t_to->id,
		t_from->team->aspace, t_to->team->aspace,
		t_from->arch_info.current_stack.ss, t_from->arch_info.current_stack.esp,
		t_to->arch_info.current_stack.ss, t_to->arch_info.current_stack.esp);
#endif
#if 0
	for(i=0; i<11; i++)
		dprintf("*esp[%d] (0x%x) = 0x%x\n", i, ((unsigned int *)new_at->esp + i), *((unsigned int *)new_at->esp + i));
#endif
	i386_set_kstack(t_to->kernel_stack_base + KSTACK_SIZE);

#if 0
{
	int a = *(int *)(t_to->kernel_stack_base + KSTACK_SIZE - 4);
}
#endif

	if(t_from->team->_aspace_id >= 0 && t_to->team->_aspace_id >= 0) {
		// they are both uspace threads
		if(t_from->team->_aspace_id == t_to->team->_aspace_id) {
			// dont change the pgdir, same address space
			new_pgdir = NULL;
		} else {
			// switching to a new address space
			new_pgdir = vm_translation_map_get_pgdir(&t_to->team->aspace->translation_map);
		}
	} else if(t_from->team->_aspace_id < 0 && t_to->team->_aspace_id < 0) {
		// they must both be kspace threads
		new_pgdir = NULL;
	} else if(t_to->team->_aspace_id < 0) {
		// the one we're switching to is kspace
		new_pgdir = vm_translation_map_get_pgdir(&t_to->team->kaspace->translation_map);
	} else {
		new_pgdir = vm_translation_map_get_pgdir(&t_to->team->aspace->translation_map);
	}
#if 0
	dprintf("new_pgdir is 0x%x\n", new_pgdir);
#endif

#if 0
{
	int a = *(int *)(t_to->arch_info.current_stack.esp - 4);
}
#endif

	if((new_pgdir % PAGE_SIZE) != 0)
		panic("arch_thread_context_switch: bad pgdir 0x%lx\n", new_pgdir);

	i386_fsave_swap(t_from->arch_info.fpu_state, t_to->arch_info.fpu_state);
	i386_context_switch(&t_from->arch_info, &t_to->arch_info, new_pgdir);
}


void
arch_thread_dump_info(void *info)
{
	struct arch_thread *at = (struct arch_thread *)info;

	dprintf("\tesp: %p\n", at->current_stack.esp);
	dprintf("\tss: %p\n", at->current_stack.ss);
	dprintf("\tfpu_state at %p\n", at->fpu_state);
}


void
arch_thread_enter_uspace(addr entry, void *args, addr ustack_top)
{
	dprintf("arch_thread_entry_uspace: entry 0x%lx, args %p, ustack_top 0x%lx\n",
		entry, args, ustack_top);

	// make sure the fpu is in a good state
	asm("fninit");

	disable_interrupts();

	i386_set_kstack(thread_get_current_thread()->kernel_stack_base + KSTACK_SIZE);

	i386_enter_uspace(entry, args, ustack_top - 4);
}


void
arch_setup_signal_frame(struct thread *t, struct sigaction *sa, int sig, int sig_mask)
{
	struct iframe *frame = t->arch_info.current_iframe;
	uint32 *stack = (uint32 *)frame->user_esp;
	uint32 *code;
	uint32 *fpu_state;
	
	if (frame->orig_eax >= 0) {
		// we're coming from a syscall
		switch (frame->eax) {
			case ERESTARTNOHAND:
				frame->eax = EINTR;
				break;
			case EINTR:
			case ERESTARTSYS:
				if (!(sa->sa_flags & SA_RESTART)) {
					frame->eax = EINTR;
					break;
				}
				/* fallthrough */
			case ERESTARTNOINTR:
				dprintf("### restarting syscall %d after signal %d\n", frame->orig_eax, sig);
				frame->eax = frame->orig_eax;
				frame->edx = frame->orig_edx;
				frame->eip -= 2;
				break;
		}
	}
	
	stack -= 192;
	code = stack + 25;
	fpu_state = stack + 64;
	
	stack[0] = (uint32)code;	// return address when sa_handler done
	stack[1] = sig;				// only argument to sa_handler
	stack[2] = frame->gs;
	stack[3] = frame->fs;
	stack[4] = frame->es;
	stack[5] = frame->ds;
	stack[6] = frame->edi;
	stack[7] = frame->esi;
	stack[8] = frame->ebp;
	stack[9] = frame->esp;
	stack[10] = frame->ebx;
	stack[11] = frame->edx;
	stack[12] = frame->ecx;
	stack[13] = frame->eax;
	stack[18] = frame->eip;
	stack[19] = frame->cs;
	stack[20] = frame->flags;
	stack[21] = frame->user_esp;
	stack[22] = frame->user_ss;
	stack[23] = sig_mask;
	stack[24] = (uint32)fpu_state;
	
	i386_fsave(fpu_state);
	
	memcpy(code, i386_return_from_signal, (i386_end_return_from_signal - i386_return_from_signal));
	
	frame->user_esp = (uint32)stack;
	frame->eip = (uint32)sa->sa_handler;
}


int64
arch_restore_signal_frame(void)
{
	struct thread *t = thread_get_current_thread();
	struct iframe *frame;
	uint32 fpu_state;
	
	dprintf("### arch_restore_signal_frame: entry\n");
	
	frame = t->arch_info.current_iframe;
	t->sig_block_mask = *((sigset_t *)((void *)frame->user_esp + sizeof(struct iframe))) & BLOCKABLE_SIGS;
	fpu_state = *((uint32 *)((void *)frame->user_esp + sizeof(struct iframe) + sizeof(uint32)));
	
	memcpy((void *)frame, (void *)frame->user_esp, sizeof(struct iframe));
	
	i386_frstor((void *)fpu_state);
	
	dprintf("### arch_restore_signal_frame: exit\n");
	
	frame->orig_eax = -1;	/* disable syscall checks */
	
	return (int64)frame->eax | ((int64)frame->edx << 32);
}


void
arch_check_syscall_restart(struct thread *t)
{
	struct iframe *frame = t->arch_info.current_iframe;
	
	dprintf("### arch_check_syscall_restart: entry\n");
	if (frame->orig_eax >= 0) {
		dprintf("### arch_check_syscall_restart: coming from a syscall\n");
		if ((frame->eax == ERESTARTNOHAND) ||
				(frame->eax == EINTR) ||
				(frame->eax == ERESTARTSYS) ||
				(frame->eax == ERESTARTNOINTR)) {
			dprintf("### arch_check_syscall_restart: syscall restart needed\n");
			frame->eax = frame->orig_eax;
			frame->edx = frame->orig_edx;
			frame->eip -= 2;
		}
	}
	dprintf("### arch_check_syscall_restart: exit\n");
}

