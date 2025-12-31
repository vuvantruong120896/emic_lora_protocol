#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include <stdint.h>

void power_service_init(void);

/* Called from main loop when there is no immediate work.
 * MVP: enters HALT().
 */
void power_service_idle(void);

#endif
