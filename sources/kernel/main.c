/*
 * Copyright (C) 2000-2005 Serge Vakulenko, <vak@cronyx.ru>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "COPY-UOS.txt" for details.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>

list_t task_active;			/* list of tasks ready to run */
task_t *task_current;			/* current running task */
task_t *task_idle;			/* background system task */
task_t *task_yelds;         //points to 1st yelded task in active tasks list
mutex_irq_t mutex_irq [ARCH_INTERRUPTS]; /* interrupt handlers */

#ifndef IDLE_TASK_STACKSZ
#if ARCH_ISR_FSPACE > 0
#define IDLE_TASK_STACKSZ   (256+ARCH_ISR_FSPACE)
#else
#define IDLE_TASK_STACKSZ   256
#endif
#endif

#define ALIGNED_IDLE_TASK_STACKSZ ((IDLE_TASK_STACKSZ + UOS_STACK_ALIGN - 1) & ~(UOS_STACK_ALIGN - 1))

static ARRAY (task_idle_data, sizeof(task_t) + ALIGNED_IDLE_TASK_STACKSZ - UOS_STACK_ALIGN);

//если надо пропустить (yeld) текущую задачу - то этот параметр = task_current
//!!! задача task_current включается в список активных ниток
unsigned task_need_schedule;


#ifndef UOS_STRICTS
#define UOS_STRICTS             0
#define UOS_STRICT_STACK        0
#endif

/*
 * Switch to most priority task if needed.
 */
CODE_ISR 
void task_schedule ()
{
	task_t *new_task;
	if (task_need_schedule != (unsigned)task_current){
	    task_yelds          = 0;
	}
	else {
	    //текущая задача в конец списка
	    /* Enqueue always puts element at the tail of the list. */
        list_append(&task_active, &task_current->item);
	    if (task_yelds == 0){
	        task_yelds = task_current;
	    }
	    if (task_yelds != (task_t*)list_first(&task_active)){
	        //отключу временно пропускаемые задачи чтобы task_policy их обошел мимо
            __list_connect_together(task_yelds->item.prev, &task_active);
	    }
	    else{
            task_yelds = 0;
	    }
	}
	new_task = task_policy ();
	if (task_yelds != 0){
        //восстановлю пропускаемые задачи обратно в активные
        __list_connect_together(task_yelds->item.prev, &task_yelds->item);
        __list_connect_together(&task_current->item, &task_active);
        if (new_task == task_idle){
            new_task = task_yelds;
            task_yelds = 0;
        }
	}
    task_need_schedule  = 0;

	if (new_task != task_current) {
		new_task->ticks++;
#if (UOS_STRICTS & UOS_STRICT_STACK) != 0
        unsigned sp = 0;
#ifdef ARCH_CONTEXT_SIZE
        sp += ARCH_CONTEXT_SIZE;
#endif
#ifdef ARCH_HAVE_FPU
#   ifdef MIPS32
        if (task_current->fpu_state != ~0)
            sp += 32*4;
#   endif
#endif
        assert2( task_stack_enough(sp), uos_assert_task_name_msg, task_current->name);
#endif //NDEBUG
		arch_task_switch (new_task);
	}
}

/*
 * Activate all waiters of the lock.
 */
CODE_ISR 
void 
mutex_activate (mutex_t *m, void *message)
{
	task_t *t;
	mutex_slot_t *s;


	assert (m != 0);
	if (! m->item.next)
		mutex_init (m);

	while (! list_is_empty (&m->waiters)) {
		t = (task_t*) list_first (&m->waiters);
		assert2 (t->wait == m
		        , "assert task %s(0x%x) wait %x activate from %x($%x)\n"
		        , t->name, (unsigned)t
		        , t->wait, m, (unsigned)message
		        );//uos_assert_task_name_msg
		t->wait = 0;
        t->message = message;
#if UOS_SIGNAL_SMART > 0
        //получили сигнал, и жду захвата мутеха, потому пермещу нитку из ожидания сигнала в 
        //  захватчик мутеха
	    if (t->MUTEX_WANT != 0)
		if (mutex_wanted_task(t))
		    continue;
#endif
		task_activate (t);
	}

	/* Activate groups. */
	list_iterate (s, &m->groups) {
		assert (s->lock == m);
		s->message = message;
		s->active = 1;
		t = s->group->waiter;
		if (t) {
		    //group_lockwait - use groun in paralel with 
		    //  lock operation
			//assert (list_is_empty (&t->item));
			s->group->waiter = 0;
#if UOS_SIGNAL_SMART > 0
            //получили сигнал, и жду захвата мутеха, потому пермещу нитку из ожидания сигнала в 
            //  захватчик мутеха
            if (t->MUTEX_WANT != 0)
            if (mutex_wanted_task(t))
                continue;
#endif
			task_activate (t);
		}
	}
}

/*
 * Additional machine-dependent initialization after
 * user initialization. Needed for some processor 
 * architectures.
 */
void __attribute__ ((weak, noinline))
uos_post_init ()
{
}



void _init_ctors(void);
void __uos_init_array();

/*
 * Call user initialization routine uos_init(),
 * then create the idle task, and run the OS.
 * The idle task uses the default system stack.
 */
int __attribute__ ((weak))
__attribute ((noreturn))
main (void)
{
	/* Create the idle task. */
	task_idle = (task_t*) task_idle_data;
	memset (task_idle->stack, STACK_MAGIC, ALIGNED_IDLE_TASK_STACKSZ);
	assert_task_good_stack (task_idle);

	/* Move stack pointer to task_idle stack area */
    unsigned sp = (unsigned)(&task_idle->stack[ALIGNED_IDLE_TASK_STACKSZ-1]);
    /* stack pointer should align to doubles */
    set_stack_pointer ((void *)( sp & ~(sizeof(double)-1) ));
#elif defined(MIPS32)
    /* MIPS32 stack pointer must be 8-byte aligned */
    unsigned long last_aligned_addr = (unsigned long) &task_idle->stack[ALIGNED_IDLE_TASK_STACKSZ-4];
    last_aligned_addr &= ~0x7;
    set_stack_pointer ((void *) last_aligned_addr);

	task_idle->name = "idle";
	list_init (&task_idle->item);
	list_init (&task_idle->slaves);
#ifdef ARCH_HAVE_FPU
	/* Default state of float point unit. */
	task_idle->fpu_state = ARCH_FPU_STATE;
#endif

	/* Make list of active tasks. */
	list_init (&task_active);
	task_current = task_idle;
	task_current->prio = task_current->base_prio = PRIO_MAX;
	task_activate (task_idle);

	/* Create user tasks. */
	uos_init ();

    __uos_init_array();
    //uos_call_global_initializers();

	/* Additional machine-dependent initialization */
	uos_post_init ();

	/* Switch to the most priority task. */
	assert (task_current == task_idle);

    task_current->prio = task_current->base_prio = 0;
    arch_state_t x = arch_intr_off();
	task_schedule ();
	arch_intr_restore(x);

	/* Idle task activity. */
	for (;;) {
		arch_idle ();
	}
}



//**********************************************************************************
//          Global Statics Initialization
//* crti.c for ARM - BPABI - use -std=c99

//***************************          ctor/dtor            ************************
typedef void (*func_ptr)(void);

#ifdef UOS_HAVE_CTORS
/*  you should place such code to linkes script to provide initialisers table
    . = ALIGN(0x4);
     __CTOR_LIST__ = .;
      LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 1)
      KEEP(*(.ctors))
      __CTOR_END__ = .;
      LONG(0)

      __DTOR_LIST__ = .;
      LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 1)
      KEEP(*(.dtors))
      __DTOR_END__ = .;
      LONG(0)
 * */

/*
 * Call global C++ constructors.
 */
__WEAK
void uos_call_global_initializers (void)
{
    typedef void (*funcptr_t) ();
    extern funcptr_t __CTOR_LIST__[];
    extern funcptr_t __CTOR_END__[];
    funcptr_t *func;

    if (__CTOR_END__[0] == 0)
    //if (__CTOR_LIST__[0] == (__CTOR_END__ - __CTOR_LIST__ - 1) )
    for (func = __CTOR_END__-1; func > __CTOR_LIST__; --func)
        (*func) ();
    //this prevents secondary initialisation
    //__CTOR_END__[0] = (funcptr_t)(~0);
}

/*
 * Call global C++ destructors.
 */
__WEAK
void uos_call_global_destructors (void)
{
    typedef void (*funcptr_t) ();
    extern funcptr_t __DTOR_LIST__[];
    extern funcptr_t __DTOR_END__[];
    funcptr_t *func;

    if (__DTOR_END__[0] == 0)
    //if (__CTOR_LIST__[0] == (__CTOR_END__ - __CTOR_LIST__ - 1) )
    for (func = __DTOR_LIST__+1; func < __DTOR_END__; ++func)
        (*func) ();
    //this prevents secondary finalisation
    //__DTOR_END__[0] = (funcptr_t)(~0);
}

#else
inline void uos_call_global_initializers(void){};
#endif


/*
 * code import from uC clibrary
 * ELF .init , .init_array/.fini_aray handling need for ARM targets,
 *      and for C++, libc code compatibility
 *      you should place such code to linkes script to provide initialisers table
   .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
 *
 * */

#ifdef UOS_HAVE_INITFINI
extern func_ptr __preinit_array_start[] __attribute__((weak));
extern func_ptr __preinit_array_end []  __attribute__((weak));
extern func_ptr __init_array_start []   __attribute__((weak));
extern func_ptr __init_array_end []     __attribute__((weak));
extern func_ptr __fini_array_start []   __attribute__((weak));
extern func_ptr __fini_array_end []     __attribute__((weak));

__attribute__((noinline, weak))
void _init(){};

__attribute__((noinline, weak))
void _fini(){};


void __uos_init_array() {
    size_t count, i;

    count = __preinit_array_end - __preinit_array_start;
    for (i = 0; i < count; i++)
        __preinit_array_start[i]();

    _init();

    count = __init_array_end - __init_array_start;
    for (i = 0; i < count; i++)
        __init_array_start[i]();
}

void __uos_fini_array() {
    size_t count, i;

    count = __preinit_array_end - __preinit_array_start;
    for (i = count - 1; i >= 0; i--)
        __fini_array_start[i]();

    _fini();
}
#else
inline void __uos_init_array(){};
#endif
