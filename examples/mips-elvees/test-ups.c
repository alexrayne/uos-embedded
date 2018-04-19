//
// Тестирование резервного питания на плате видеорегистратора.
//
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>

#define UPS_IRQ     3

ARRAY (task_space, 0x400);

mutex_t ups_mutex;

void task (void *arg)
{
    mutex_lock_irq(&ups_mutex, UPS_IRQ, 0, 0);
    
    debug_printf("Waiting for UPS interrupt...\n");

    for (;;) {
		mutex_wait(&ups_mutex);
		
		for (;;) {
			debug_putchar(0, '*');
			mdelay(10);
		}
    }
}

void uos_init (void)
{
    debug_printf("\n\nTesting UPS\n");
    
	task_create( task, "task", "task", 2, task_space, sizeof(task_space) );
}
