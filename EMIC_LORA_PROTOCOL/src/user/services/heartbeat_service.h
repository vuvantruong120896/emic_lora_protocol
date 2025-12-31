#ifndef HEARTBEAT_SERVICE_H
#define HEARTBEAT_SERVICE_H

#include <stdint.h>

void heartbeat_service_init(void);

/* Called when link reports heartbeat due. */
void heartbeat_service_send(void);

#endif
