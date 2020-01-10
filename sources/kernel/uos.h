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
#ifndef __KERNEL_UOS_H_
#define	__KERNEL_UOS_H_ 1

#include <uos-conf.h>
#include <runtime/sys/uosc.h>
#include <runtime/list.h>

#ifdef __cplusplus
extern "C" {
#endif

/**\~russian
 * RECURSIVE_LOCKS задает стиль мутекса
 *  = 0 - ближайший unlock освобождает мутекс
 *  = 1 - мутекс отслеживает количество блокировок в текущей задаче и 
 *          на каждый вызов lock должен быть произведен unlock  
 * */
#ifndef RECURSIVE_LOCKS
#   define RECURSIVE_LOCKS	1
#endif

/**\~russian
 * FASTER_LOCKS добавляет код ускоряющий захват уже захваченого мутекса
 *  = 0 - всякий захват требует выхода в защищенный от шедулера режим 
 *  = 1 - если текущая задача уже захватила мутекс, все выполняется бустрее.
 *      но чуть разбухает размер кода
 * */
#ifndef FASTER_LOCKS
#   define FASTER_LOCKS  1
#endif

/* System data structures. */
typedef struct _array_t array_t;
typedef struct _task_t task_t;
typedef struct _mutex_t mutex_t;
typedef struct _mutex_irq_t mutex_irq_t;

/* Lock group data structures. */
typedef struct _mutex_group_t mutex_group_t;
typedef struct _mutex_slot_t mutex_slot_t;

/* Fast irq handler. */
typedef bool_t (*handler_t) (void*);

/* Task management. */
task_t *task_create (void (*func)(void*), void *arg, const char *name, int priority,
	array_t *stack, unsigned stacksz)  __cpp_decls;
void task_exit (void *status) __attribute__ ((__noreturn__))  __cpp_decls;
void task_delete (task_t *task, void *status) __cpp_decls;
void *task_wait (task_t *task) __cpp_decls;
int task_stack_avail (task_t *task) __cpp_decls;
//* return safe-checked task name.
//* return (damaged) on suspicious string
const char *task_name (task_t *task) __cpp_decls;
int task_priority (task_t *task) __cpp_decls;
void task_set_priority (task_t *task, int priority) __cpp_decls;
void *task_private (task_t *task) __cpp_decls;
void task_set_private (task_t *task, void *privatep) __cpp_decls;
void task_yield (void)  __cpp_decls;

struct _stream_t;
void task_print (struct _stream_t *stream, task_t *t);
void tasks_print(struct _stream_t *stream);
void mutex_print (struct _stream_t *stream, mutex_t *m);

#ifndef ARCH_HAVE_FPU
//__attribute__ ((error ("no float point coprocessor: ARCH_HAVE_FPU required")))
#endif
unsigned int task_fpu_control (task_t *t, unsigned int mode, unsigned int mask);

//****************************************************************************
/* Initialize a data structure of the lock. */
void mutex_init (mutex_t *)  __cpp_decls;

/* Lock management. */
void mutex_lock (mutex_t *lock)  __cpp_decls;
void mutex_unlock (mutex_t *lock)  __cpp_decls;

/**\~english
 * Try to get the lock. Return 1 on success, 0 on failure.
 * The calling task does not block.
 * In the case the lock has associated IRQ number,
 * after acquiring the lock the IRQ will be disabled.
 */
bool_t mutex_trylock (mutex_t *lock)  __cpp_decls;
INLINE bool_t mutex_is_locked (mutex_t *m);
INLINE bool_t mutex_is_wait (mutex_t *m);
INLINE bool_t mutex_is_my (mutex_t *m);

/*! this is function that called from schdeler-switcher context, it should not use
 *   os routines that can cause task block 
 * 
 */
typedef bool_t (*scheduless_condition)(void* arg);

/** this lock is blocks until <waitfor> return true, or mutex locked.
 * \return true - mutex succesfuly locked
 * \return false - if mutex not locked due to <waitfor> signalled
 * */
bool_t mutex_lock_until (mutex_t *lock, scheduless_condition waitfor, void* waitarg)  __cpp_decls;

void mutex_signal (mutex_t *lock, void *message)  __cpp_decls;
void *mutex_wait (mutex_t *lock)  __cpp_decls;
/*
 * for owned mutex:
 * \return true - if signaled and therefore valid task_current->message
 *          false - if timeout. !!! this case can loose mutex owning.
 *                      should check that lock is locked there
 * for not owned mutex - just wait for thread activate and returns waitfor state
 *
 * */
bool_t mutex_wait_until (mutex_t *lock, scheduless_condition waitfor, void* waitarg)  __cpp_decls;

/* Interrupt management. */
void mutex_lock_irq (mutex_t*, int irq, handler_t func, void *arg) __cpp_decls;
/**\~english
 *  this allows instead IRQ_EVT use desired isr-fast-handler routine for signaling
       to ordinar mutexes, and so realize Software Interrupt similar to system 
       Hardware Interrupts. 
   Fast handler of swi (mutex_irq_t.handler(arg)) executes in context of 
       signaling thread, and cause task scheduling only if handler gives !=0 message
   \~rusian
   вместо железного события, здесь используется обработчик софтверного прерывания
       при этом быстрый обработчик выполняется в контексте сигналящей нитки,
       и сигнал к мутексу проникнет если он возвратит сообщение != 0 
 */
void mutex_lock_swi (mutex_t*, mutex_irq_t* swi, handler_t func, void *arg) __cpp_decls;
void mutex_unlock_irq (mutex_t*) __cpp_decls;
void mutex_attach_irq (mutex_t *m, int irq, handler_t func, void *arg) __cpp_decls;
void mutex_attach_swi (mutex_t *m, mutex_irq_t* swi, handler_t func, void *arg) __cpp_decls;
void mutex_dettach_irq(mutex_t *m) __cpp_decls;

/* Group management. */
mutex_group_t *mutex_group_init (array_t *buf, unsigned buf_size) __cpp_decls;
bool_t mutex_group_add (mutex_group_t*, mutex_t*) __cpp_decls;
bool_t mutex_group_remove (mutex_group_t*, mutex_t*) __cpp_decls;
void mutex_group_listen (mutex_group_t*) __cpp_decls;
/** \~russian
 * сбрасывает статус активности с мутехов группы, ожидание активности
 * ведется с этого момента.
 * подключает прослушивание не подключенных мутехов
 */
void mutex_group_relisten(mutex_group_t*) __cpp_decls;
void mutex_group_unlisten (mutex_group_t*) __cpp_decls;
void mutex_group_wait (mutex_group_t *g, mutex_t **lock_ptr, void **msg_ptr) __cpp_decls;
/**\~russian
 * этот лок ожидает захвата мутекса или сигнала от группы
 * \return true - захвачен lock
 * \return false - получен сигнал от группы, или мутекс был закрыт извне
 * */
bool_t mutex_group_lockwaiting (mutex_t *lock, mutex_group_t *g, mutex_t **lock_ptr, void **msg_ptr) __cpp_decls;

/* User-supplied startup routine. */
extern void uos_init (void) __cpp_decls;



/*
 * ----------
 * | Lock   |
 * | ~~~~   |
 * | master --> T
 * |	    |
 * | waiters--> T -> T...
 * |	    |
 * | slaves --> T -> T -> T...
 * ---------- <-/----/----/
 */
struct _mutex_t {
	list_t		item;		/* double linked list pointers */
	task_t *	master;		/* task, acquired the lock */
	list_t		waiters;	/* tasks, stopped on `wait' */
	list_t		slaves;		/* tasks, waiting for lock */
	list_t		groups;		/* group slots, waiting for signal */
	mutex_irq_t *	irq;		/* irq, associated with the lock */
	int		prio;		/* current lock priority */
#if RECURSIVE_LOCKS
	small_int_t	deep;		/* recursive locking deep */
#endif
};

struct _mutex_irq_t {
    mutex_t *   lock;       /* lock, associated with this irq */
    handler_t   handler;    /* fast interrupt handler */
    void *      arg;        /* argument for fast handler */
    small_int_t irq;        /* irq number */
    bool_t      pending;    /* interrupt is pending */
};

/*
 * Slot: a group element.
 */
struct _mutex_slot_t {
	list_t		item;		/* double linked list pointers */
	mutex_group_t *	group;		/* parent group */
	mutex_t *	lock;		/* link to the related lock */
	void *		message;	/* return value for mutex_group_wait() */
	bool_t		active;		/* 1 when lock was signal()led  */
};

/*
 * Group: an array of slots.
 */
struct _mutex_group_t {
//	mutex_t		lock;		/* lock to group_wait() on it */
	task_t *	waiter;		/* the waiting task pointer */
	small_uint_t	size;		/* size of slot[] array */
	small_uint_t	num;		/* number of elements in slot[] */
	mutex_slot_t	slot [1];	/* array of slots is placed here */
};

/*
 * Opaque array, well aligned.
 * Used for allocating task stacks and lock groups.
 */
struct _array_t {
	void *data;
};

#define ARRAY(name, bytes) \
	array_t name [((bytes) + sizeof (array_t) - 1) / sizeof (array_t)]

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})



//* anonimous type should be only static, cause it cant be referenced anywhere else
//  static
#define MUTEX_GROUP(n) \
    union {\
        ARRAY (data, sizeof(mutex_group_t) + (n-1) * sizeof(mutex_slot_t));\
        mutex_group_t g;\
        struct { \
            mutex_group_t   g;\
            mutex_slot_t    s[n-1];\
        } field;\
    }


INLINE
bool_t mutex_is_locked (mutex_t *m){
    return (m->master != (void*)0 );
}

INLINE
bool_t mutex_is_wait (mutex_t *m){
    return     !list_is_empty (&m->waiters) 
            || !list_is_empty (&m->groups)
            || (m->irq != (void*)0);
}

/* Current running task. */
extern task_t *task_current;

INLINE
bool_t mutex_is_my (mutex_t *m){
    return (m->master == task_current);
}

#ifdef __cplusplus
}
#endif

#endif /* !__SYS_UOS_H_ */
