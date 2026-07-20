#include "object_bms.h"
#include <string.h>

/* Example telemetry object for the Linux reference client. */

#define BMS_OBJECT_ID 33000
#define BMS_RESOURCE_VOLTAGE 0

typedef struct
{
    lwm2m_list_t list;
    double voltage;
} bms_instance_t;

static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId,
                        int *numDataP, lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP)
{
    bms_instance_t *targetP;
    int i;

    (void)contextP;

    targetP = (bms_instance_t *)lwm2m_list_find(
        objectP->instanceList,
        instanceId
    );

    if (targetP == NULL)
        return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(1);

        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;

        *numDataP = 1;
        (*dataArrayP)[0].id = BMS_RESOURCE_VOLTAGE;
    }

    for (i = 0; i < *numDataP; i++)
    {
        if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
            return COAP_404_NOT_FOUND;

        switch ((*dataArrayP)[i].id)
        {
        case BMS_RESOURCE_VOLTAGE:
            lwm2m_data_encode_float(
                targetP->voltage,
                *dataArrayP + i
            );
            break;

        default:
            return COAP_404_NOT_FOUND;
        }
    }
    return COAP_205_CONTENT;
}

lwm2m_object_t *get_bms_object(void)
{
    lwm2m_object_t *objectP;
    bms_instance_t *instanceP;

    objectP = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
    if(objectP == NULL)
        return NULL;

    memset(objectP, 0, sizeof(lwm2m_object_t));
    objectP->objID = BMS_OBJECT_ID;
    objectP->versionMajor = 1;
    objectP->versionMinor = 0;

    instanceP = (bms_instance_t *)lwm2m_malloc(sizeof(bms_instance_t));

    if (instanceP == NULL)
    {
        lwm2m_free(objectP);
        return NULL;
    }

    memset(instanceP, 0, sizeof(bms_instance_t));

    instanceP->list.id = 0;
    instanceP->voltage = 12.7;

    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);
    objectP->readFunc = prv_read;

    return objectP;
}

void free_bms_object(lwm2m_object_t *objectP)
{
    if (objectP == NULL)
        return;

    LWM2M_LIST_FREE(objectP->instanceList);

    lwm2m_free(objectP);
}
