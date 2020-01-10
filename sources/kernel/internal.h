/*
 * The internal uOS definitions.
 * Not for the end user.
 *
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
#ifndef __KERNEL_INTERNAL_H_
#define	__KERNEL_INTERNAL_H_ 1

#include <runtime/arch.h>
#include <runtime/assert.h>
#include <kernel/uos.h>

#ifdef __AVR__
#	include <kernel/avr/machdep.h>
#endif
#if defined (__arm__) || defined (__thumb__)
#   if defined (ARM_CORTEX_M1) || defined (ARM_CORTEX_M3) || \
        defined (ARM_CORTEX_M4)
#	    include <kernel/cortex-m/machdep.h>
#   else
#	    include <kernel/arm/machdep.h>
#   endif
#endif
#ifdef I386
#	include <kernel/i386/machdep.h>
#endif
#ifdef MIPS32
#	include <kernel/mips/machdep.h>
#endif
#ifdef MSP430
#	include <kernel/msp430/machdep.h>
#endif
#ifdef LINUX386
#	include <kernel/linux386/machdep.h>
#endif


#ifndef INLINE
#define INLINE inline static 
#endif

#ifdef __cplusplus
extern "C" {
#endif


//**************    marchdeps    ***************************** 
#ifndef ARCH_intr_off

INLINE 
__attribute__ ((always_inline))
arch_state_t arch_intr_off (void) {
    arch_state_t x;
    arch_intr_disable (&x);
    return x;
}

#else
#define arch_intr_off() ARCH_intr_off()
#endif //!ARCH_intr_off



/*
 * ----------
 * | Task   |
 * | ~~~~   |
 * | lock ----> M
 * |	    |
 * | wait ----> M
 * |	    |
 * | slaves --> M -> M -> M...
 * ---------- <-/----/----/
 */
#define UOS_STACK_ALIGN     sizeof(void*)
#ifndef UOS_SIGNAL_SMART
#define UOS_SIGNAL_SMART 0
#endif

#if UOS_SIGNAL_SMART == 1
#define MUTEX_WANT want
#elif UOS_SIGNAL_SMART > 0
#define MUTEX_WANT lock
#endif

struct _task_t {
	list_t		item;		/* double linked list pointers */
	mutex_t *	lock;		/* lock, blocking the task */
#if UOS_SIGNAL_SMART == 1
    mutex_t *   want;       /* lock, that task want to lock*/
#endif
	mutex_t *	wait;		/* lock, the task is waiting for */
	list_t		slaves;		/* locks, acquired by task */
	void *		message;	/* return value for mutex_wait() */
	void *		privatep;	/* private task data pointer */
	const char *	name;		/* printable task name */
	small_int_t	base_prio;	/* initial task priority */
	small_int_t	prio;		/* current task priority */
	arch_stack_t	stack_context;	/* saved sp when not running */
	mutex_t		finish;		/* lock to wait on for task finished */
	unsigned long	ticks;		/* a number of switches to the task */
#ifdef ARCH_HAVE_FPU
	arch_fpu_t	fpu_state;	/* per-task state of FP coprocessor */
#endif
	unsigned char stack [1]	/* stack area is placed here */
		__attribute__((aligned(UOS_STACK_ALIGN)));
};

#define PRIO_MAX ( (1ul<<(INT_SIZE*8-1))-1 )

/* The table of interrupt handlers. */
extern mutex_irq_t mutex_irq [];

/* Current running task. */
extern task_t *task_current;

/* Global flag set to 1 when update task_active */
extern unsigned task_need_schedule;

/* Special `idle' task. */
extern task_t *task_idle;

/* List of tasks ready to run. */
extern list_t task_active;

extern const char uos_assert_task_name_msg[];
extern const char uos_assert_mutex_task_name_msg[];



/* Switch to most priority task. */
void task_schedule (void) __noexcept __NOTHROW;

/* Activate all waiters of the lock. */
void mutex_activate (mutex_t *m, void *message) __noexcept __NOTHROW;

// it is try to handle IRQ handler and then mutex_activate,
//  if handler return true
// \return 0 - no activation was pended
// \return else - value of handler:
bool_t mutex_awake (mutex_t *m, void *message) __noexcept __NOTHROW;

// assign current task to m->slaves and schdule. priority adjusted
void mutex_slaved_yield(mutex_t *m);
void mutex_slave_task(mutex_t *m, task_t* t);

#if UOS_SIGNAL_SMART > 0
// check that task wants t->MUTEX_WANT mutex, and sequenced to lock on it
// \return 0 - task no need to slaved on mutex
// \return 1 - task wanted mutex and slaved on it
bool_t mutex_wanted_task(task_t* t);
#endif


// assign current task to m->master
INLINE void mutex_do_lock(mutex_t *m)
{
#if RECURSIVE_LOCKS
    assert (m->deep == 0);
#endif
    m->master = task_current;

    /* Put this lock into the list of task slaves. */
    assert(task_current->slaves.next != 0);
    list_append (&task_current->slaves, &m->item);

    /* Update the value of task priority.
     * It must be the maximum of base priority,
     * and all slave lock priorities. */
    if (task_current->prio < m->prio)
        task_current->prio = m->prio;
}

//just lock mutex if it is free. not MT-safe, must call in sheduler-locked context
INLINE 
__attribute__ ((always_inline))
bool_t mutex_trylock_in (mutex_t *m){
    if (! m->master){
        assert2 (list_is_empty (&m->slaves)
                , uos_assert_mutex_task_name_msg
                , m, ((task_t *)list_first(&m->slaves))->name
                );
        mutex_do_lock(m);
    }
    if (m->master == task_current){
#if RECURSIVE_LOCKS
    ++m->deep;
#endif
        return 1;
    }
    else
        return 0;
}

//wait mutex free and lock
INLINE bool_t mutex_lock_yiedling(mutex_t *m)
{
    assert2 ((task_current->lock == 0), uos_assert_mutex_task_name_msg, m, (task_current->name));
    while (m->master && m->master != task_current) {
        /* Monitor is locked, block the task. */
        mutex_slaved_yield(m);
    }
    return mutex_trylock_in(m);
}

//wait mutex free and lock
//* \return 1 - lock ok
//*         0 - lock break by waitfor
INLINE bool_t mutex_lock_yiedling_until(mutex_t *m
        , scheduless_condition waitfor, void* waitarg
        )
{
    assert2 ((task_current->lock == 0), uos_assert_mutex_task_name_msg, m, (task_current->name));
    while (m->master && m->master != task_current) {
        if (waitfor != 0)
        if ((*waitfor)(waitarg)) {
            task_current->lock = 0;
            return 0;
        }
        /* Monitor is locked, block the task. */
        mutex_slaved_yield(m);
    }
    return mutex_trylock_in(m);
}

INLINE 
__attribute__ ((always_inline))
bool_t mutex_recurcived_lock(mutex_t *m)
{
#if FASTER_LOCKS > 0
    if (m->master == task_current){
#if RECURSIVE_LOCKS
    ++m->deep;
#endif
        return 1;
    }
#endif //FASTER_LOCKS
    return 0;
}

CODE_ISR 
INLINE bool_t mutex_check_pended_irq (mutex_t *m)
{
    /* On pending irq, we must call fast handler. */
    if (m->irq) {
        mutex_irq_t *   irq = m->irq;
        if (irq->pending) {
            irq->pending = 0;

            if (irq->handler != 0) {
                if ((irq->handler) (irq->arg) == 0){
                    /* Unblock all tasks, waiting for irq. */
                    mutex_activate (m, (void*)(irq->irq));
                    return 1;
                }
            }
            //всеже разрешаем прерывания без обработчика только при их ожидании mutex_wait
            //else if (irq->irq >= 0)
            //        arch_intr_allow (irq->irq);
        }//if (irq->pending)
    }
    // WARN TODO - это принципиальная несовместимость с мутехами базовй uOS.
    //        В базовой версии ожидающий мутех сам разрешает свои прерывания.
    //        В этой версии - контроль прерываний целиком в ведении юзера.
    //        arch_intr_allow (irq->irq);
    return 0;
}

void mutex_do_unlock(mutex_t *m);

/* Recalculate task priority, based on priorities of acquired locks. */
void task_recalculate_prio (task_t *t) __noexcept __NOTHROW;

/* Recalculate lock priority, based on priorities of waiting tasks. */
void mutex_recalculate_prio (mutex_t *m) __noexcept __NOTHROW;

/* Utility functions. */
INLINE bool_t task_is_waiting (task_t *task) {
	return (task->lock || task->wait);
}

/* \~russian
 * почти тоже самое что task_activate, только без ограничений.
 * используется для активации поллинга ожидающей\блокированой нитки
 * для того чтобы она могла проверить свои условия блокировки.
 * см. timer/etimer.c
 */
CODE_ISR 
INLINE void task_awake (task_t *task) {
    list_prepend(&task_active, &task->item); //list_append
	if (task_current->prio < task->prio)
		task_need_schedule = 1;
}

CODE_ISR 
INLINE void task_activate (task_t *task) {
    assert (! task_is_waiting (task));
    task_awake(task);
}

INLINE
bool_t task_is_active (task_t *task)
{
    task_t * t;
    list_iterate(t, &task_active){
        if (t == task)
            return 1;
    }
    return 0;
}


/* Task policy, e.g. the scheduler.
 * Task_active contains a list of all tasks, which are ready to run.
 * Find a task with the biggest priority. */
INLINE 
__attribute__ ((always_inline))
CODE_ISR 
task_t *task_policy (void)
{
	task_t *t, *r;

	r = task_idle;
	list_iterate (t, &task_active) {
		if (t->prio > r->prio)
			r = t;
	}
	return r;
}

// this tasks yelds, so try to avoid them
extern task_t *task_yelds;

#define STACK_MAGIC		0xaau

#define STACK_GUARD(x)		((x)->stack[0] == STACK_MAGIC)

#ifndef NDEBUG
#define assert_task_good_stack(t) \
    assert2 (STACK_GUARD (t), uos_assert_task_name_msg, (t)->name)

#else
#define assert_task_good_stack(t)
#endif

INLINE
bool_t  task_stack_enough(unsigned preserve){
    unsigned sp = (unsigned)arch_get_stack_pointer ();
    if ( sp <= (unsigned long)task_current->stack_limit)
    return ( (sp-preserve-(ARCH_CONTEXT_SIZE+ARCH_ISR_FSPACE)) >= (unsigned)(task_current->stack) ) ;
    return 0;
}


#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_INTERNAL_H_ */
