#include "liblwm2m.h"

void *lwm2m_connect_server(uint16_t securityInstanceId, void *userData)
{
    (void)securityInstanceId;
    (void)userData;

    return NULL;
}

void lwm2m_close_connection(void *sessionH, void *userData)
{
    (void)sessionH;
    (void)userData;
}