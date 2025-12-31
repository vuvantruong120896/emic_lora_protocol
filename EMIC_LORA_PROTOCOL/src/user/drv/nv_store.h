#ifndef NV_STORE_H
#define NV_STORE_H

#include <stdint.h>

void nv_store_init(void);

uint32_t nv_store_get_fcnt_up(void);
void nv_store_set_fcnt_up(uint32_t v);

uint16_t nv_store_get_last_alarm_id(void);
void nv_store_set_last_alarm_id(uint16_t v);

#endif
