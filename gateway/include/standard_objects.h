#ifndef OTA_GATEWAY_STANDARD_OBJECTS_H
#define OTA_GATEWAY_STANDARD_OBJECTS_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

lwm2m_object_t *get_security_object(
    int serverId,
    const char *serverUri,
    char *pskId,
    char *psk,
    uint16_t pskLength,
    bool isBootstrap
);

char *get_server_uri(lwm2m_object_t *objectP, uint16_t securityInstanceId);

void clean_security_object(lwm2m_object_t *objectP);

lwm2m_object_t *get_server_object(
    int serverId,
    const char *binding,
    int lifetime,
    bool storing
);

void clean_server_object(lwm2m_object_t *objectP);

lwm2m_object_t *get_object_device(void);

void free_object_device(lwm2m_object_t *objectP);

#ifdef __cplusplus
}
#endif

#endif