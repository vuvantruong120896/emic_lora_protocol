#include "nv_store.h"

/* MVP: RAM-backed storage.
 * Replace with data-flash/NVM integration in e2studio later.
 */

static uint32_t s_fcnt_up;
static uint16_t s_last_alarm_id;

void nv_store_init(void)
{
    s_fcnt_up = 1UL;
    s_last_alarm_id = 0U;
}

uint32_t nv_store_get_fcnt_up(void)
{
    return s_fcnt_up;
}

void nv_store_set_fcnt_up(uint32_t v)
{
    s_fcnt_up = v;
}

uint16_t nv_store_get_last_alarm_id(void)
{
    return s_last_alarm_id;
}

void nv_store_set_last_alarm_id(uint16_t v)
{
    s_last_alarm_id = v;
}
