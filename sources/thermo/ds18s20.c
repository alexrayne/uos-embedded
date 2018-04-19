#include <runtime/lib.h>
#include <kernel/uos.h>
#include <thermo/ds18s20.h>

static void poll(ds18s20_t *ds)
{
    static uint8_t cmd[2];
    static int16_t ans;
    static int res;

    res = ow_reset(ds->ow);
    if (res != OW_ERR_OK) {
        ds->cur_temp = 0x7FFFFFFF + THERMO_ERR_IO;
        return;
    }

    cmd[0] = 0xCC; // Skip ROM
    cmd[1] = 0x44; // Convert T
    res = ow_trx(ds->ow, cmd, 2, 0, 0);
    if (res != OW_ERR_OK) {
        ds->cur_temp = 0x7FFFFFFF + THERMO_ERR_IO;
        return;
    }

    timer_delay(ds->timer, ds->poll_period - 10);

    res = ow_reset(ds->ow);
    if (res != OW_ERR_OK) {
        ds->cur_temp = 0x7FFFFFFF + THERMO_ERR_IO;
        return;
    }

    cmd[0] = 0xCC; // Skip ROM
    cmd[1] = 0xBE; // Read Scratchpad
    res = ow_trx(ds->ow, cmd, 2, 0, 0);
    if (res != OW_ERR_OK) {
        ds->cur_temp = 0x7FFFFFFF + THERMO_ERR_IO;
        return;
    }

    timer_delay(ds->timer, 10);

    res = ow_trx(ds->ow, 0, 0, &ans, 2);
    if (res != OW_ERR_OK) {
        ds->cur_temp = 0x7FFFFFFF + THERMO_ERR_IO;
        return;
    }

    ds->cur_temp = ans * 500;
    
    ow_reset(ds->ow);
}


static int ds18s20_read(thermoif_t *thermo)
{
    ds18s20_t *ds = (ds18s20_t *) thermo;
    
#ifdef DS18S20_DIRECT_POLLING
    poll(ds);
#endif

    return ds->cur_temp;
}

#ifndef DS18S20_DIRECT_POLLING
ARRAY(poll_stack, 800);

void poll_task(void *arg)
{
    ds18s20_t *ds = arg;
    for (;;) {
        poll(ds);
    }
}
#endif

void ds18s20_init(ds18s20_t *ds18s20, owif_t *ow, timer_t *timer, int poll_period_ms, int poll_prio)
{
    ds18s20->ow = ow;
    ds18s20->timer = timer;
    ds18s20->poll_period = poll_period_ms;
    ds18s20->thermoif.read = ds18s20_read;
    
#ifndef DS18S20_DIRECT_POLLING
    task_create(poll_task, ds18s20, "poll_ds18s20", poll_prio, poll_stack, sizeof(poll_stack));
#endif
}
