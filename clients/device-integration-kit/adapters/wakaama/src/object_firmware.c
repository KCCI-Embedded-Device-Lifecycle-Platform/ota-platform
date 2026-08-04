#include "object_firmware.h"

#include <string.h>

/*
 * Wakaama adapter for LwM2M Firmware Update Object 1.2.
 *
 * State and Update Result can be read, but firmware transfer and installation
 * are rejected until Firmware Update Service is connected.
 */

#define FIRMWARE_RESOURCE_PACKAGE 0
#define FIRMWARE_RESOURCE_PACKAGE_URI 1
#define FIRMWARE_RESOURCE_UPDATE 2
#define FIRMWARE_RESOURCE_STATE 3
#define FIRMWARE_RESOURCE_UPDATE_RESULT 5
#define FIRMWARE_RESOURCE_PROTOCOL_SUPPORT 8
#define FIRMWARE_RESOURCE_DELIVERY_METHOD 9
#define FIRMWARE_PROTOCOL_COAP_INSTANCE_ID 0
#define FIRMWARE_PROTOCOL_COAP 0

#define FIRMWARE_STATE_IDLE 0
#define FIRMWARE_UPDATE_RESULT_INITIAL 0
#define FIRMWARE_DELIVERY_METHOD_PULL_ONLY 0

typedef struct
{
    lwm2m_list_t list;
    int64_t state;
    int64_t updateResult;
} firmware_instance_t;

static uint8_t prv_read_protocol_support(lwm2m_data_t *dataP)
{
    lwm2m_data_t *protocolP;

    if(dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
    {
        if(dataP->value.asChildren.count != 1||
           dataP->value.asChildren.array == NULL ||
           dataP->value.asChildren.array[0].id !=
            FIRMWARE_PROTOCOL_COAP_INSTANCE_ID)
        {
            return COAP_404_NOT_FOUND;
        }
        protocolP = dataP->value.asChildren.array;
    }
    else
    {
        protocolP = lwm2m_data_new(1);
        
        if(protocolP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;
        
        protocolP[0].id = FIRMWARE_PROTOCOL_COAP_INSTANCE_ID;
        lwm2m_data_encode_instances(protocolP, 1, dataP);
    }

    lwm2m_data_encode_int(FIRMWARE_PROTOCOL_COAP, protocolP);

    return COAP_205_CONTENT;
}

static uint8_t prv_read(
    lwm2m_context_t *contextP,
    uint16_t instanceId,
    int *numDataP,
    lwm2m_data_t **dataArrayP,
    lwm2m_object_t *objectP)
{
    firmware_instance_t *instanceP;
    int index;

    (void)contextP;

    instanceP = (firmware_instance_t *)lwm2m_list_find(
        objectP->instanceList,
        instanceId
    );

    if (instanceP == NULL)
        return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(4);

        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;

        *numDataP = 4;
        (*dataArrayP)[0].id = FIRMWARE_RESOURCE_STATE;
        (*dataArrayP)[1].id = FIRMWARE_RESOURCE_UPDATE_RESULT;
        (*dataArrayP)[2].id = FIRMWARE_RESOURCE_PROTOCOL_SUPPORT;
        (*dataArrayP)[3].id = FIRMWARE_RESOURCE_DELIVERY_METHOD;
    }

    for (index = 0; index < *numDataP; index++)
    {
        if ((*dataArrayP)[index].type == LWM2M_TYPE_MULTIPLE_RESOURCE &&
            (*dataArrayP)[index].id != FIRMWARE_RESOURCE_PROTOCOL_SUPPORT)
            return COAP_404_NOT_FOUND;

        switch ((*dataArrayP)[index].id)
        {
        case FIRMWARE_RESOURCE_STATE:
            lwm2m_data_encode_int(
                instanceP->state,
                *dataArrayP + index
            );
            break;

        case FIRMWARE_RESOURCE_UPDATE_RESULT:
            lwm2m_data_encode_int(
                instanceP->updateResult,
                *dataArrayP + index
            );
            break;
        
        case FIRMWARE_RESOURCE_DELIVERY_METHOD:
            lwm2m_data_encode_int(
                FIRMWARE_DELIVERY_METHOD_PULL_ONLY,
                *dataArrayP + index
            );
            break;
        
        case FIRMWARE_RESOURCE_PROTOCOL_SUPPORT:
        {
            uint8_t result = prv_read_protocol_support(*dataArrayP + index);

            if (result != COAP_205_CONTENT)
                return result;
            break;
        }

        case FIRMWARE_RESOURCE_PACKAGE:
        case FIRMWARE_RESOURCE_PACKAGE_URI:
        case FIRMWARE_RESOURCE_UPDATE:
            return COAP_405_METHOD_NOT_ALLOWED;

        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_write(
    lwm2m_context_t *contextP,
    uint16_t instanceId,
    int numData,
    lwm2m_data_t *dataArray,
    lwm2m_object_t *objectP,
    lwm2m_write_type_t writeType)
{
    firmware_instance_t *instanceP;
    int index;

    (void)contextP;
    (void)writeType;

    instanceP = (firmware_instance_t *)lwm2m_list_find(
        objectP->instanceList,
        instanceId
    );

    if (instanceP == NULL)
        return COAP_404_NOT_FOUND;

    if (numData <= 0 || dataArray == NULL)
        return COAP_400_BAD_REQUEST;

    for (index = 0; index < numData; index++)
    {
        if (dataArray[index].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
            return COAP_404_NOT_FOUND;

        switch (dataArray[index].id)
        {
        case FIRMWARE_RESOURCE_PACKAGE:
        case FIRMWARE_RESOURCE_PACKAGE_URI:
            return COAP_501_NOT_IMPLEMENTED;

        default:
            return COAP_405_METHOD_NOT_ALLOWED;
        }
    }

    return COAP_501_NOT_IMPLEMENTED;
}

static uint8_t prv_execute(
    lwm2m_context_t *contextP,
    uint16_t instanceId,
    uint16_t resourceId,
    uint8_t *buffer,
    int length,
    lwm2m_object_t *objectP)
{
    firmware_instance_t *instanceP;

    (void)contextP;
    (void)buffer;

    instanceP = (firmware_instance_t *)lwm2m_list_find(
        objectP->instanceList,
        instanceId
    );

    if (instanceP == NULL)
        return COAP_404_NOT_FOUND;

    if (resourceId != FIRMWARE_RESOURCE_UPDATE)
        return COAP_405_METHOD_NOT_ALLOWED;

    if (length != 0)
        return COAP_400_BAD_REQUEST;

    return COAP_501_NOT_IMPLEMENTED;
}

lwm2m_object_t *get_firmware_update_object(void)
{
    lwm2m_object_t *objectP;
    firmware_instance_t *instanceP;

    objectP = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (objectP == NULL)
        return NULL;

    memset(objectP, 0, sizeof(lwm2m_object_t));
    objectP->objID = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;
    objectP->versionMajor = 1;
    objectP->versionMinor = 2;

    instanceP = (firmware_instance_t *)lwm2m_malloc(
        sizeof(firmware_instance_t)
    );

    if (instanceP == NULL)
    {
        lwm2m_free(objectP);
        return NULL;
    }

    memset(instanceP, 0, sizeof(firmware_instance_t));
    instanceP->list.id = 0;
    instanceP->state = FIRMWARE_STATE_IDLE;
    instanceP->updateResult = FIRMWARE_UPDATE_RESULT_INITIAL;

    objectP->instanceList = LWM2M_LIST_ADD(
        objectP->instanceList,
        instanceP
    );
    objectP->readFunc = prv_read;
    objectP->writeFunc = prv_write;
    objectP->executeFunc = prv_execute;

    return objectP;
}

void free_firmware_update_object(lwm2m_object_t *objectP)
{
    if (objectP == NULL)
        return;

    LWM2M_LIST_FREE(objectP->instanceList);
    lwm2m_free(objectP);
}
