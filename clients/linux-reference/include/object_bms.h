#ifndef OTA_LINUX_REFERENCE_CLIENT_OBJECT_BMS_H
#define OTA_LINUX_REFERENCE_CLIENT_OBJECT_BMS_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

lwm2m_object_t *get_bms_object(void);
void free_bms_object(lwm2m_object_t *objectP);

#ifdef __cplusplus
}
#endif

#endif
