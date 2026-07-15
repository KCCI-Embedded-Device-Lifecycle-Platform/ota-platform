#include "liblwm2m.h"

#define BMS_OBJECT_ID 33000
#define BMS_RESOURCE_VOLTAGE 0

typedef struct
{
    lwm2m_list_t list;
    double voltage;
}bms_instanct_t;