#ifndef __UOS_INTERFACE_H__
#define __UOS_INTERFACE_H__

#include <loadable/loadable.h>
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#include <timer/timeout.h>
#include <rtc/time.h>
#include <buf/ring.h>
#include <gpio/debouncer.h>
#include <gpio/int_debouncer.h>
#include <stream/stream.h>
#include <usb/usbdev.h>
#include <random/rand15.h>
#include <loadable/loader.h>
#include "uos-ft-config.h"

typedef struct _uos_loadable_t uos_loadable_t;
struct _uos_loadable_t
{
	loadable_t base;
	
#ifdef FT_UOS_assert
    void (* assert_fail) (const char *expr, const char *file,
		unsigned line, const char *func);
#endif

//
// loadable/loader.h
//
#ifdef FT_UOS_loader_first_loadable
	loadable_t * (* loader_first_loadable)(unsigned long loadable_id);
#endif
#ifdef FT_UOS_loader_next_loadable
	loadable_t * (* loader_next_loadable)(loadable_t *current,
		unsigned long loadable_id);
#endif
#ifdef FT_UOS_loader_load
	int (* loader_load)(loadable_t *loadable);
#endif
#ifdef FT_UOS_loader_unload
	int (* loader_unload)(loadable_t *loadable);
#endif
#ifdef FT_UOS_loader_init
	int (* loader_init)(loadable_t *loadable);
#endif
#ifdef FT_UOS_loader_load_all
	int (* loader_load_all)();
#endif
#ifdef FT_UOS_loader_unload_all
	int (* loader_unload_all)();
#endif
#ifdef FT_UOS_loader_init_all
	int (* loader_init_all)();
#endif

//
// runtime/lib.h
//
#ifdef FT_UOS_khz
	unsigned long (* khz)();
#endif
#ifdef FT_UOS_qsort
	void (* qsort) (void *a, size_t n, size_t es,
		int (*cmp)(const void*, const void*));
#endif
#ifdef FT_UOS_bsearch
	void * (* bsearch) (const void *key, const void *base, size_t nmemb,
		size_t size, int (*compar) (const void *, const void *));
#endif
#ifdef FT_UOS_strstr
	unsigned char * (* strstr) (const char *haystack,
		const unsigned char *needle);
#endif
#ifdef FT_UOS_strspn
	int (* strspn) (const unsigned char *s, const unsigned char *accept);
#endif
#ifdef FT_UOS_strcspn
	int (* strcspn) (const unsigned char *s, const unsigned char *reject);
#endif
#ifdef FT_UOS_debug_putchar
	void (* debug_putchar)(void *arg, short c);
#endif
#ifdef FT_UOS_debug_putc
	void (* debug_putc) (char c);
#endif
#ifdef FT_UOS_debug_getchar
	unsigned short (* debug_getchar) (void);
#endif
#ifdef FT_UOS_debug_peekchar
	int (* debug_peekchar) (void);
#endif
#ifdef FT_UOS_debug_puts
	void (* debug_puts) (const char *str);
#endif
#ifdef FT_UOS_debug_printf
	int (* debug_printf)(const char *fmt, ...);
#endif
#ifdef FT_UOS_debug_dump
	void (* debug_dump) (const char *caption, void* data, unsigned len);
#endif
#ifdef FT_UOS_debug_dump_stack
	void (* debug_dump_stack) (const char *caption, void *sp,
		void* frame, void *callee);
#endif
#ifdef FT_UOS_udelay
	void (* udelay) (small_uint_t);
#endif
#ifdef FT_UOS_mdelay
	void (* mdelay) (small_uint_t);
#endif
#ifdef FT_UOS_uos_halt
	void (* uos_halt) (int);
#endif
#ifdef FT_UOS_memchr
	void * (* memchr)(const void *src_void, unsigned char c, size_t length);
#endif
#ifdef FT_UOS_memcmp
	small_int_t (* memcmp)(const void *m1, const void *m2, size_t n);
#endif
#ifdef FT_UOS_memcpy
	void * (* memcpy)(void *dst0, const void *src0, size_t len0);
#endif
#ifdef FT_UOS_memmove
	void * (* memmove)(void *dest, const void *src, size_t n);
#endif
#ifdef FT_UOS_memset
	void * (* memset)(void *m, unsigned char c, size_t n);
#endif

//
// kernel/internal.h
//
#ifdef FT_UOS_mutex_activate
	void (* mutex_activate) (mutex_t *m, void *message);
#endif
#ifdef FT_UOS_arch_intr_allow 
	void (* arch_intr_allow) (int irq);
#endif

//
// kernel/uos.h
//
#ifdef FT_UOS_task_create
	task_t * (* task_create) (void (*func)(void*), void *arg, const char *name,
		int priority, array_t *stack, unsigned stacksz);
#endif
#ifdef FT_UOS_task_exit
	void (* task_exit) (void *status);
#endif
#ifdef FT_UOS_task_delete
	void (* task_delete) (task_t *task, void *status);
#endif
#ifdef FT_UOS_task_wait
	void * (* task_wait) (task_t *task);
#endif
#ifdef FT_UOS_task_stack_avail
	int (* task_stack_avail) (task_t *task);
#endif
#ifdef FT_UOS_task_name
	const char * (* task_name) (task_t *task);
#endif
#ifdef FT_UOS_task_priority
	int (* task_priority) (task_t *task);
#endif
#ifdef FT_UOS_task_set_priority
	void (* task_set_priority) (task_t *task, int priority);
#endif
#ifdef FT_UOS_task_private
	void * (* task_private) (task_t *task);
#endif
#ifdef FT_UOS_task_set_private
	void (* task_set_private) (task_t *task, void *privatep);
#endif
#ifdef FT_UOS_task_yield
	void (* task_yield) (void);
#endif
#ifdef FT_UOS_task_print
	void (* task_print) (struct _stream_t *stream, task_t *t);
#endif
#ifdef FT_UOS_task_fpu_control
	unsigned int (* task_fpu_control) (task_t *t, unsigned int mode,
		unsigned int mask);
#endif
#ifdef FT_UOS_mutex_lock
	void (* mutex_lock) (mutex_t *lock);
#endif
#ifdef FT_UOS_mutex_unlock
	void (* mutex_unlock) (mutex_t *lock);
#endif
#ifdef FT_UOS_mutex_trylock
	bool_t (* mutex_trylock) (mutex_t *lock);
#endif
#ifdef FT_UOS_mutex_signal
	void (* mutex_signal) (mutex_t *lock, void *message);
#endif
#ifdef FT_UOS_mutex_wait
	void * (* mutex_wait) (mutex_t *lock);
#endif
#ifdef FT_UOS_mutex_lock_irq
	void (* mutex_lock_irq) (mutex_t*, int irq, handler_t func, void *arg);
#endif
#ifdef FT_UOS_mutex_unlock_irq
	void (* mutex_unlock_irq) (mutex_t*);
#endif
#ifdef FT_UOS_mutex_attach_irq
	void (* mutex_attach_irq) (mutex_t *m, int irq, handler_t func, void *arg);
#endif
#ifdef FT_UOS_mutex_group_init
	mutex_group_t * (* mutex_group_init) (array_t *buf, unsigned buf_size);
#endif
#ifdef FT_UOS_mutex_group_add
	bool_t (* mutex_group_add) (mutex_group_t*, mutex_t*);
#endif
#ifdef FT_UOS_mutex_group_listen
	void (* mutex_group_listen) (mutex_group_t*);
#endif
#ifdef FT_UOS_mutex_group_unlisten
	void (* mutex_group_unlisten) (mutex_group_t*);
#endif
#ifdef FT_UOS_mutex_group_wait
	void (* mutex_group_wait) (mutex_group_t *g, mutex_t **lock_ptr,
		void **msg_ptr);
#endif

//
// timer/timer.h
//
#ifdef FT_UOS_timer_init_ns
	void (* timer_init_ns) (timer_t *t, unsigned long khz,
		unsigned long nsec_per_tick);
#endif
#ifdef FT_UOS_timer_init_us
	void (* timer_init_us) (timer_t *t, unsigned long khz,
		unsigned long usec_per_tick);
#endif
#ifdef FT_UOS_timer_init
	void (* timer_init) (timer_t *t, unsigned long khz,
		small_uint_t msec_per_tick);
#endif
#ifdef FT_UOS_timer_update
	void (* timer_update) (timer_t *t);
#endif
#ifdef FT_UOS_timer_delay
	void (* timer_delay) (timer_t *t, unsigned long msec);
#endif
#ifdef FT_UOS_timer_milliseconds
	unsigned long (* timer_milliseconds) (timer_t *t);
#endif
#ifdef FT_UOS_timer_microseconds
	uint64_t (* timer_microseconds) (timer_t *t);
#endif
#ifdef FT_UOS_timer_days
	unsigned int (* timer_days) (timer_t *t, unsigned long *milliseconds);
#endif
#ifdef FT_UOS_timer_passed
	bool_t (* timer_passed) (timer_t *t, unsigned long t0, unsigned int msec);
#endif
#ifdef FT_UOS_interval_greater_or_equal
	bool_t (* interval_greater_or_equal) (long interval, long msec);
#endif
#ifdef FT_UOS_timer_set_period_ns
	void (* timer_set_period_ns) (timer_t *t, unsigned long nsec_per_tick);
#endif
#ifdef FT_UOS_timer_set_period_us
	void (* timer_set_period_us) (timer_t *t, unsigned long usec_per_tick);
#endif
#ifdef FT_UOS_timer_set_period
	void (* timer_set_period) (timer_t *t, small_uint_t msec_per_tick);
#endif

//
// timer/timeout.h
//
#ifdef FT_UOS_timeout_init
	void (* timeout_init) (timeout_t *to, timer_t *timer, mutex_t *mutex);
#endif
#ifdef FT_UOS_timeout_set_signal
	void (* timeout_set_signal) (timeout_t *to, void *signal);
#endif
#ifdef FT_UOS_timeout_set_value
	void (* timeout_set_value) (timeout_t *to, unsigned long interval_msec);
#endif
#ifdef FT_UOS_timeout_set_value_us
	void (* timeout_set_value_us) (timeout_t *to, unsigned long long interval_usec);
#endif
#ifdef FT_UOS_timeout_set_value_ns
	void (* timeout_set_value_ns) (timeout_t *to, unsigned long long interval_nsec);
#endif
#ifdef FT_UOS_timeout_set_autoreload
	void (* timeout_set_autoreload) (timeout_t *to, int autoreload);
#endif
#ifdef FT_UOS_timeout_set_handler
	void (* timeout_set_handler) (timeout_t *to,
		timeout_handler handler, void *arg);
#endif
#ifdef FT_UOS_timeout_start
	void (* timeout_start) (timeout_t *to);
#endif
#ifdef FT_UOS_timeout_stop
	void (* timeout_stop) (timeout_t *to);
#endif

//
// rtc/time.h
//
#ifdef FT_UOS_datetime_to_time_t
	time_t (* datetime_to_time_t)(const datetime_t *dt);
#endif
#ifdef FT_UOS_time_t_to_datetime
	void (* time_t_to_datetime)(time_t time, datetime_t *dt);
#endif
#ifdef FT_UOS_datetime_to_ntp
	ntp_time_t (* datetime_to_ntp)(const datetime_t *dt);
#endif
#ifdef FT_UOS_ntp_to_datetime
	void (* ntp_to_datetime)(ntp_time_t ntp_time, datetime_t *dt);
#endif

//
// buf/ring.h
//
#ifdef FT_UOS_ring_init
	void (* ring_init)(ring_t *r, void *buffer, unsigned size);
#endif
#ifdef FT_UOS_ring_avail_write
	unsigned (* ring_avail_write)(ring_t *r);
#endif
#ifdef FT_UOS_ring_avail_read
	unsigned (* ring_avail_read)(ring_t *r);
#endif
#ifdef FT_UOS_ring_write_ptr
	void * (* ring_write_ptr)(ring_t *r);
#endif
#ifdef FT_UOS_ring_read_ptr
	void * (* ring_read_ptr)(ring_t *r);
#endif
#ifdef FT_UOS_ring_advance_write_ptr
	unsigned (* ring_advance_write_ptr)(ring_t *r, unsigned len);
#endif
#ifdef FT_UOS_ring_advance_read_ptr
	unsigned (* ring_advance_read_ptr)(ring_t *r, unsigned len);
#endif
#ifdef FT_UOS_ring_write
	unsigned (* ring_write)(ring_t *r, void *data, unsigned len);
#endif
#ifdef FT_UOS_ring_read
	unsigned (* ring_read)(ring_t *r, void *data, unsigned len);
#endif

//
// gpio/debouncer.h
//
#ifdef FT_UOS_debouncer_init
	void (* debouncer_init)(debouncer_t *db, int hist_len, int init_val);
#endif
#ifdef FT_UOS_debouncer_add
	void (* debouncer_add)(debouncer_t *db, int new_val);
#endif
#ifdef FT_UOS_debouncer_value
	int (* debouncer_value)(debouncer_t *db);
#endif
#ifdef FT_UOS_debouncer_reset
	int (* debouncer_reset)(debouncer_t *db, int init_val);
#endif

//
// gpio/int_debouncer.h
//
#ifdef FT_UOS_int_debouncer_init
	void (* int_debouncer_init)(int_debouncer_t *db, int init_val);
#endif
#ifdef FT_UOS_int_debouncer_add
	void (* int_debouncer_add)(int_debouncer_t *db, int new_val);
#endif
#ifdef FT_UOS_int_debouncer_value
	int (* int_debouncer_value)(int_debouncer_t *db);
#endif
#ifdef FT_UOS_int_debouncer_reset
	int (* int_debouncer_reset)(int_debouncer_t *db, int init_val);
#endif

//
// stream/stream.h
//
#ifdef FT_UOS_drain_input
	void (* drain_input) (stream_t *u);
#endif
#ifdef FT_UOS_stream_puts
	int (* stream_puts) (stream_t *u, const char *str);
#endif
#ifdef FT_UOS_stream_gets
	unsigned char * (*stream_gets) (stream_t *u, unsigned char *str, int len);
#endif
#ifdef FT_UOS_stream_printf
	int (* stream_printf) (stream_t *u, const char *fmt, ...);
#endif
#ifdef FT_UOS_stream_vprintf
	int (* stream_vprintf) (stream_t *u, const char *fmt, va_list args);
#endif
#ifdef FT_UOS_stream_scanf
	int (* stream_scanf) (stream_t *u, const char *fmt, ...);
#endif
#ifdef FT_UOS_stream_vscanf
	int (* stream_vscanf) (stream_t *u, const char *fmt, va_list argp);
#endif
#ifdef FT_UOS_snprintf
	int (* snprintf) (unsigned char *buf, int size, const char *fmt, ...);
#endif
#ifdef FT_UOS_vsnprintf
	int (* vsnprintf) (unsigned char *buf, int size, const char *fmt, va_list args);
#endif
#ifdef FT_UOS_sscanf
	int (* sscanf) (const unsigned char *buf, const char *fmt, ...);
#endif
#ifdef FT_UOS_stropen
	stream_t * (* stropen) (stream_buf_t *u, unsigned char *buf, int size);
#endif
#ifdef FT_UOS_strclose
	void (* strclose) (stream_buf_t *u);
#endif
#ifdef FT_UOS_vprintf_getlen
	int (* vprintf_getlen) (const char *fmt, va_list args);
#endif

//
// usb/usbdev.h
//
#ifdef FT_UOS_usbdevhal_bind
	void (* usbdevhal_bind) (usbdev_t *u, usbdev_hal_t *hal, void *arg, mutex_t *hal_mutex);
#endif
#ifdef FT_UOS_usbdevhal_reset
	void (* usbdevhal_reset) (usbdev_t *u);
#endif
#ifdef FT_UOS_usbdevhal_suspend
	void (* usbdevhal_suspend) (usbdev_t *u);
#endif
#ifdef FT_UOS_usbdevhal_in_done
	void (* usbdevhal_in_done) (usbdev_t *u, unsigned ep, int size);
#endif
#ifdef FT_UOS_usbdevhal_out_done
	void (* usbdevhal_out_done) (usbdev_t *u, unsigned ep, int trans_type, void *data, int size);
#endif
#ifdef FT_UOS_usbdev_sof_done
	void (* usbdev_sof_done) (usbdev_t *u, unsigned ep_n);
#endif
#ifdef FT_UOS_usbdev_init
	void (* usbdev_init) (usbdev_t *u, mem_pool_t *pool, const usb_dev_desc_t *dd);
#endif
#ifdef FT_UOS_usbdev_add_config_desc
	void (* usbdev_add_config_desc) (usbdev_t *u, const void *cd);
#endif
#ifdef FT_UOS_usbdev_set_string_table
	void (* usbdev_set_string_table) (usbdev_t *u, const void *st[]);
#endif
#ifdef FT_UOS_usbdev_set_dev_specific_handler
	void (* usbdev_set_dev_specific_handler) (usbdev_t *u, usbdev_specific_t handler, void *tag);
#endif
#ifdef FT_UOS_usbdev_set_iface_specific_handler
	void (* usbdev_set_iface_specific_handler) (usbdev_t *u, unsigned if_n, usbdev_specific_t handler, void *tag);
#endif
#ifdef FT_UOS_usbdev_set_ep_specific_handler
	void (* usbdev_set_ep_specific_handler) (usbdev_t *u, unsigned ep_n, int dir, usbdev_specific_t handler, void *tag);
#endif
#ifdef FT_UOS_usbdev_set_ack_handler
	void (* usbdev_set_ack_handler) (usbdev_t *u, unsigned ep_n, int dir, usbdev_ack_t handler, void *tag);
#endif
#ifdef FT_UOS_usbdev_remove_ack_handler
	void (* usbdev_remove_ack_handler) (usbdev_t *u, unsigned ep_n, int dir);
#endif
#ifdef FT_UOS_usbdev_ack_in
	void (* usbdev_ack_in) (usbdev_t *u, unsigned ep_n, const void *data, int size);
#endif
#ifdef FT_UOS_usbdev_set_ack
	void (* usbdev_set_ack) (usbdev_t *u, unsigned ep_n);
#endif
#ifdef FT_UOS_usbdev_recv
	int  (* usbdev_recv) (usbdev_t *u, unsigned ep_n, void *data, int size);
#endif
#ifdef FT_UOS_usbdev_activate_ep
	void (* usbdev_activate_ep) (usbdev_t *u, unsigned ep_n);
#endif
#ifdef FT_UOS_usbdev_deactivate_ep
	void (* usbdev_deactivate_ep) (usbdev_t *u, unsigned ep_n);
#endif

//
// random/rand15.h
//
#ifdef FT_UOS_rand15
    short (* rand15) (void);
#endif
#ifdef FT_UOS_srand15
    void (* srand15) (unsigned short);
#endif

};

#endif // __UOS_INTERFACE_H__
