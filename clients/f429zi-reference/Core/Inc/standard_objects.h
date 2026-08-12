#ifndef F429ZI_REFERENCE_STANDARD_OBJECTS_H
#define F429ZI_REFERENCE_STANDARD_OBJECTS_H

#include "liblwm2m.h"

lwm2m_object_t *get_security_object(
    int server_id,
    const char *server_uri,
    char *psk_id,
    char *psk,
    uint16_t psk_length,
    bool is_bootstrap);

void clean_security_object(lwm2m_object_t *object);

lwm2m_object_t *get_server_object(
    int server_id,
    const char *binding,
    int lifetime,
    bool storing);

void clean_server_object(lwm2m_object_t *object);

lwm2m_object_t *get_object_device(void);
void free_object_device(lwm2m_object_t *object);

#endif
