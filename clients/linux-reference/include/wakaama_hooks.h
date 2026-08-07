#ifndef OTA_LINUX_REFERENCE_CLIENT_WAKAAMA_HOOKS_H
#define OTA_LINUX_REFERENCE_CLIENT_WAKAAMA_HOOKS_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "tinydtls/connection.h"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
#endif

typedef struct
{
    lwm2m_context_t *lwm2mContextP;
    lwm2m_object_t *securityObjectP;
    int socketFd;
    lwm2m_dtls_connection_t *connectionList;
    int addressFamily;
} wakaama_client_context_t;


#endif
