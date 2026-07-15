#ifndef OTA_GATEWAY_WAKAAMA_HOOKS_H
#define OTA_GATEWAY_WAKAAMA_HOOKS_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "udp/connection.h"

#ifdef __cplusplus
}
#endif

typedef struct
{
    lwm2m_object_t *securityObjectP;
    int socketFd;
    lwm2m_connection_t *connectionList;
    int addressFamily;
    const char *serverHost;
    const char *serverPort;

} gateway_client_context_t;


#endif