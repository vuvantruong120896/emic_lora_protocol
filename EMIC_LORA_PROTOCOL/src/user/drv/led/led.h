#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum
{
    LED_ID_RED = 0,
    LED_ID_GREEN = 1
} led_id_t;

void led_init(void);
void led_set(led_id_t id, uint8_t on);
void led_toggle(led_id_t id);
void led_all_off(void);

#endif /* LED_H */
