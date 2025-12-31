#include "heartbeat_service.h"

#include "../link/lora_link.h"

void heartbeat_service_init(void)
{
}

void heartbeat_service_send(void)
{
    lora_link_send_heartbeat();
}
