#include "uos-interface.h"

DECLARE_LOADABLE(uos_loadable_t, LOADABLE_ID_UOS, 1, LOADABLE_DEFAULT_LOADER,
    UOS_MAJVER, UOS_MINVER, UOS_FT_MAJVER, UOS_FT_MINVER, 0, 0)

#ifdef FT_UOS_assert
#   ifdef NDEBUG
        .assert_fail = __assert_fail_ndebug,
#   else
        .assert_fail = __assert_fail,
#   endif
#endif

//
// loadable/loader.h
//
#ifdef FT_UOS_loader_first_loadable
    .loader_first_loadable = loader_first_loadable,
#endif
#ifdef FT_UOS_loader_next_loadable
    .loader_next_loadable = loader_next_loadable,
#endif
#ifdef FT_UOS_loader_load
    .loader_load = loader_load,
#endif
#ifdef FT_UOS_loader_unload
    .loader_unload = loader_unload,
#endif
#ifdef FT_UOS_loader_load_all
    .loader_load_all = loader_load_all,
#endif
#ifdef FT_UOS_loader_unload_all
    .loader_unload_all = loader_unload_all,
#endif

//
// runtime/lib.h
//
#ifdef FT_UOS_khz
    .khz = khz,
#endif
#ifdef FT_UOS_qsort
    .qsort = qsort,
#endif
#ifdef FT_UOS_bsearch
    .bsearch = bsearch,
#endif
#ifdef FT_UOS_strstr
    .strstr = strstr,
#endif
#ifdef FT_UOS_strspn
    .strspn = strspn,
#endif
#ifdef FT_UOS_strcspn
    .strcspn = strcspn,
#endif
#ifdef FT_UOS_debug_putchar
    .debug_putchar = debug_putchar,
#endif
#ifdef FT_UOS_debug_putc
    .debug_putc = debug_putc,
#endif
#ifdef FT_UOS_debug_getchar
    .debug_getchar = debug_getchar,
#endif
#ifdef FT_UOS_debug_peekchar
    .debug_peekchar = debug_peekchar,
#endif
#ifdef FT_UOS_debug_puts
    .debug_puts = debug_puts,
#endif
#ifdef FT_UOS_debug_printf
    .debug_printf = debug_printf,
#endif
#ifdef FT_UOS_debug_dump
    .debug_dump = debug_dump,
#endif
#ifdef FT_UOS_debug_dump_stack
    .debug_dump_stack = debug_dump_stack,
#endif
#ifdef FT_UOS_udelay
    .udelay = udelay,
#endif
#ifdef FT_UOS_mdelay
    .mdelay = mdelay,
#endif
#ifdef FT_UOS_uos_halt
    .uos_halt = uos_halt,
#endif
#ifdef FT_UOS_memchr
    .memchr = memchr,
#endif
#ifdef FT_UOS_memcmp
    .memcmp = memcmp,
#endif
#ifdef FT_UOS_memcpy
    .memcpy = memcpy,
#endif
#ifdef FT_UOS_memmove
    .memmove = memmove,
#endif
#ifdef FT_UOS_memset
    .memset = memset,
#endif

//
// kernel/internal.h
//
#ifdef FT_UOS_mutex_activate
    .mutex_activate = mutex_activate,
#endif
#ifdef FT_UOS_arch_intr_allow
    .arch_intr_allow = arch_intr_allow,
#endif

//
// kernel/uos.h
//
#ifdef FT_UOS_task_create
    .task_create = task_create,
#endif
#ifdef FT_UOS_task_exit
    .task_exit = task_exit,
#endif
#ifdef FT_UOS_task_delete
    .task_delete = task_delete,
#endif
#ifdef FT_UOS_task_wait
    .task_wait = task_wait,
#endif
#ifdef FT_UOS_task_stack_avail
    .task_stack_avail = task_stack_avail,
#endif
#ifdef FT_UOS_task_name
    .task_name = task_name,
#endif
#ifdef FT_UOS_task_priority
    .task_priority = task_priority,
#endif
#ifdef FT_UOS_task_set_priority
    .task_set_priority = task_set_priority,
#endif
#ifdef FT_UOS_task_private
    .task_private = task_private,
#endif
#ifdef FT_UOS_task_set_private
    .task_set_private = task_set_private,
#endif
#ifdef FT_UOS_task_yield
    .task_yield = task_yield,
#endif
#ifdef FT_UOS_task_print
    .task_print = task_print,
#endif
#ifdef FT_UOS_task_fpu_control
    .task_fpu_control = task_fpu_control,
#endif
#ifdef FT_UOS_mutex_lock
    .mutex_lock = mutex_lock,
#endif
#ifdef FT_UOS_mutex_unlock
    .mutex_unlock = mutex_unlock,
#endif
#ifdef FT_UOS_mutex_trylock
    .mutex_trylock = mutex_trylock,
#endif
#ifdef FT_UOS_mutex_signal
    .mutex_signal = mutex_signal,
#endif
#ifdef FT_UOS_mutex_wait
    .mutex_wait = mutex_wait,
#endif
#ifdef FT_UOS_mutex_lock_irq
    .mutex_lock_irq = mutex_lock_irq,
#endif
#ifdef FT_UOS_mutex_unlock_irq
    .mutex_unlock_irq = mutex_unlock_irq,
#endif
#ifdef FT_UOS_mutex_attach_irq
    .mutex_attach_irq = mutex_attach_irq,
#endif
#ifdef FT_UOS_mutex_group_init
    .mutex_group_init = mutex_group_init,
#endif
#ifdef FT_UOS_mutex_group_add
    .mutex_group_add = mutex_group_add,
#endif
#ifdef FT_UOS_mutex_group_listen
    .mutex_group_listen = mutex_group_listen,
#endif
#ifdef FT_UOS_mutex_group_unlisten
    .mutex_group_unlisten = mutex_group_unlisten,
#endif
#ifdef FT_UOS_mutex_group_wait
    .mutex_group_wait = mutex_group_wait,
#endif

//
// timer/timer.h
//
#ifdef FT_UOS_timer_init_ns
    .timer_init_ns = timer_init_ns,
#endif
#ifdef FT_UOS_timer_init_us
    .timer_init_us = timer_init_us,
#endif
#ifdef FT_UOS_timer_init
    .timer_init = timer_init,
#endif
#ifdef FT_UOS_timer_update
    .timer_update = timer_update,
#endif
#ifdef FT_UOS_timer_delay
    .timer_delay = timer_delay,
#endif
#ifdef FT_UOS_timer_milliseconds
    .timer_milliseconds = timer_milliseconds,
#endif
#ifdef FT_UOS_timer_microseconds
    .timer_microseconds = timer_microseconds,
#endif
#ifdef FT_UOS_timer_days
    .timer_days = timer_days,
#endif
#ifdef FT_UOS_timer_passed
    .timer_passed = timer_passed,
#endif
#ifdef FT_UOS_interval_greater_or_equal
    .interval_greater_or_equal = interval_greater_or_equal,
#endif
#ifdef FT_UOS_timer_set_period_ns
    .timer_set_period_ns = timer_set_period_ns,
#endif
#ifdef FT_UOS_timer_set_period_us
    .timer_set_period_us = timer_set_period_us,
#endif
#ifdef FT_UOS_timer_set_period
    .timer_set_period = timer_set_period,
#endif

//
// timer/timeout.h
//
#ifdef FT_UOS_timeout_init
    .timeout_init = timeout_init,
#endif
#ifdef FT_UOS_timeout_set_signal
    .timeout_set_signal = timeout_set_signal,
#endif
#ifdef FT_UOS_timeout_set_value
    .timeout_set_value = timeout_set_value,
#endif
#ifdef FT_UOS_timeout_set_value_us
    .timeout_set_value_us = timeout_set_value_us,
#endif
#ifdef FT_UOS_timeout_set_value_ns
    .timeout_set_value_ns = timeout_set_value_ns,
#endif
#ifdef FT_UOS_timeout_set_autoreload
    .timeout_set_autoreload = timeout_set_autoreload,
#endif
#ifdef FT_UOS_timeout_set_handler
    .timeout_set_handler = timeout_set_handler,
#endif
#ifdef FT_UOS_timeout_start
    .timeout_start = timeout_start,
#endif
#ifdef FT_UOS_timeout_stop
    .timeout_stop = timeout_stop,
#endif

//
// rtc/time.h
//
#ifdef FT_UOS_datetime_to_time_t
	.datetime_to_time_t = datetime_to_time_t,
#endif
#ifdef FT_UOS_time_t_to_datetime
	.time_t_to_datetime = time_t_to_datetime,
#endif
#ifdef FT_UOS_datetime_to_ntp
	.datetime_to_ntp = datetime_to_ntp,
#endif
#ifdef FT_UOS_ntp_to_datetime
	.ntp_to_datetime = ntp_to_datetime,
#endif

//
// buf/ring.h
//
#ifdef FT_UOS_ring_init
    .ring_init = ring_init,
#endif
#ifdef FT_UOS_ring_avail_write
    .ring_avail_write = ring_avail_write,
#endif
#ifdef FT_UOS_ring_avail_read
    .ring_avail_read = ring_avail_read,
#endif
#ifdef FT_UOS_ring_write_ptr
    .ring_write_ptr = ring_write_ptr,
#endif
#ifdef FT_UOS_ring_read_ptr
    .ring_read_ptr = ring_read_ptr,
#endif
#ifdef FT_UOS_ring_advance_write_ptr
    .ring_advance_write_ptr = ring_advance_write_ptr,
#endif
#ifdef FT_UOS_ring_advance_read_ptr
    .ring_advance_read_ptr = ring_advance_read_ptr
#endif
#ifdef FT_UOS_ring_write
    .ring_write = ring_write,
#endif
#ifdef FT_UOS_ring_read
    .ring_read = ring_read,
#endif

//
// gpio/debouncer.h
//
#ifdef FT_UOS_debouncer_init
	.debouncer_init = debouncer_init,
#endif
#ifdef FT_UOS_debouncer_add
	.debouncer_add = debouncer_add,
#endif
#ifdef FT_UOS_debouncer_value
	.debouncer_value = debouncer_value,
#endif
#ifdef FT_UOS_debouncer_reset
	.debouncer_reset = debouncer_reset,
#endif

//
// gpio/int_debouncer.h
//
#ifdef FT_UOS_int_debouncer_init
	.int_debouncer_init = int_debouncer_init,
#endif
#ifdef FT_UOS_int_debouncer_add
	.int_debouncer_add = int_debouncer_add,
#endif
#ifdef FT_UOS_int_debouncer_value
	.int_debouncer_value = int_debouncer_value,
#endif
#ifdef FT_UOS_int_debouncer_reset
	.int_debouncer_reset = int_debouncer_reset,
#endif

//
// stream/stream.h
//
#ifdef FT_UOS_drain_input
	.drain_input = drain_input,
#endif
#ifdef FT_UOS_stream_puts
	.stream_puts = stream_puts,
#endif
#ifdef FT_UOS_stream_gets
	.stream_gets = stream_gets,
#endif
#ifdef FT_UOS_stream_printf
	.stream_printf = stream_printf,
#endif
#ifdef FT_UOS_stream_vprintf
	.stream_vprintf = stream_vprintf,
#endif
#ifdef FT_UOS_stream_scanf
	.stream_scanf = stream_scanf,
#endif
#ifdef FT_UOS_stream_vscanf
	.stream_vscanf = stream_vscanf,
#endif
#ifdef FT_UOS_snprintf
	.snprintf = snprintf,
#endif
#ifdef FT_UOS_vsnprintf
	.vsnprintf = vsnprintf,
#endif
#ifdef FT_UOS_sscanf
	.sscanf = sscanf,
#endif
#ifdef FT_UOS_stropen
	.stropen = stropen,
#endif
#ifdef FT_UOS_strclose
	.strclose = strclose,
#endif
#ifdef FT_UOS_vprintf_getlen
	.vprintf_getlen = vprintf_getlen,
#endif

//
// usb/usbdev.h
//
#ifdef FT_UOS_usbdevhal_bind
	.usbdevhal_bind = usbdevhal_bind,
#endif
#ifdef FT_UOS_usbdevhal_reset
	.usbdevhal_reset = usbdevhal_reset,
#endif
#ifdef FT_UOS_usbdevhal_suspend
	.usbdevhal_suspend = usbdevhal_suspend,
#endif
#ifdef FT_UOS_usbdevhal_in_done
	.usbdevhal_in_done = usbdevhal_in_done,
#endif
#ifdef FT_UOS_usbdevhal_out_done
	.usbdevhal_out_done = usbdevhal_out_done,
#endif
#ifdef FT_UOS_usbdev_sof_done
	.usbdev_sof_done = usbdev_sof_done,
#endif
#ifdef FT_UOS_usbdev_init
	.usbdev_init = usbdev_init,
#endif
#ifdef FT_UOS_usbdev_add_config_desc
	.usbdev_add_config_desc = usbdev_add_config_desc,
#endif
#ifdef FT_UOS_usbdev_set_string_table
	.usbdev_set_string_table = usbdev_set_string_table,
#endif
#ifdef FT_UOS_usbdev_set_dev_specific_handler
	.usbdev_set_dev_specific_handler = usbdev_set_dev_specific_handler,
#endif
#ifdef FT_UOS_usbdev_set_iface_specific_handler
	.usbdev_set_iface_specific_handler = usbdev_set_iface_specific_handler,
#endif
#ifdef FT_UOS_usbdev_set_ep_specific_handler
	.usbdev_set_ep_specific_handler = usbdev_set_ep_specific_handler,
#endif
#ifdef FT_UOS_usbdev_set_ack_handler
	.usbdev_set_ack_handler = usbdev_set_ack_handler,
#endif
#ifdef FT_UOS_usbdev_remove_ack_handler
	.usbdev_remove_ack_handler = usbdev_remove_ack_handler,
#endif
#ifdef FT_UOS_usbdev_ack_in
	.usbdev_ack_in = usbdev_ack_in,
#endif
#ifdef FT_UOS_usbdev_set_ack
	.usbdev_set_ack = usbdev_set_ack,
#endif
#ifdef FT_UOS_usbdev_recv
	.usbdev_recv = usbdev_recv,
#endif
#ifdef FT_UOS_usbdev_activate_ep
	.usbdev_activate_ep = usbdev_activate_ep,
#endif
#ifdef FT_UOS_usbdev_deactivate_ep
	.usbdev_deactivate_ep = usbdev_deactivate_ep,
#endif

//
// random/rand15.h
//
#ifdef FT_UOS_rand15
    .rand15 = rand15,
#endif
#ifdef FT_UOS_srand15
    .srand15 = srand15,
#endif


END_DECLARE_LOADABLE
