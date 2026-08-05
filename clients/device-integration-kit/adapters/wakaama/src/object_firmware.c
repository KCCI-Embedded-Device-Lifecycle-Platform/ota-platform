#include "object_firmware.h"

#include <string.h>

/*
 * Wakaama adapter for LwM2M Firmware Update Object 1.2.
 *
 * Bridges /5 resources to Firmware Update Service and the optional
 * Download Transport. Package push remains unsupported.
 */

#define FIRMWARE_RESOURCE_PACKAGE 0
#define FIRMWARE_RESOURCE_PACKAGE_URI 1
#define FIRMWARE_RESOURCE_UPDATE 2
#define FIRMWARE_RESOURCE_STATE 3
#define FIRMWARE_RESOURCE_UPDATE_RESULT 5
#define FIRMWARE_RESOURCE_PROTOCOL_SUPPORT 8
#define FIRMWARE_RESOURCE_DELIVERY_METHOD 9
#define FIRMWARE_RESOURCE_CANCEL 10
#define FIRMWARE_RESOURCE_SEVERITY 11
#define FIRMWARE_RESOURCE_MAXIMUM_DEFER_PERIOD 13
#define FIRMWARE_PROTOCOL_COAP_INSTANCE_ID 0
#define FIRMWARE_PROTOCOL_COAP 0

#define FIRMWARE_DELIVERY_METHOD_PULL_ONLY 0

typedef struct
{
    lwm2m_list_t list;
    firmware_update_service_t *service;
    firmware_download_transport_t *download_transport;
} firmware_instance_t;

static uint8_t prv_map_service_status(firmware_update_service_status_t service_status)
{
    switch (service_status)
    {
    case FIRMWARE_UPDATE_SERVICE_STATUS_OK:
        return COAP_204_CHANGED;

    case FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT:
        return COAP_400_BAD_REQUEST;

    case FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE:
    case FIRMWARE_UPDATE_SERVICE_STATUS_NOT_ALLOWED:
        return COAP_405_METHOD_NOT_ALLOWED;

    case FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE:
    default:
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
}

static uint8_t prv_map_download_transport_status(
    firmware_download_transport_status_t transport_status)
{
    switch (transport_status)
    {
    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK:
        return COAP_204_CHANGED;

    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI:
    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL:
        return COAP_400_BAD_REQUEST;

    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE:
    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE:
    default:
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
}

static firmware_update_download_failure_t prv_map_download_failure(
    firmware_download_transport_status_t transport_status)
{
    switch (transport_status)
    {
    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE:
        return FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST;

    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI:
        return FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INVALID_URI;

    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL:
        return FIRMWARE_UPDATE_DOWNLOAD_FAILURE_UNSUPPORTED_PROTOCOL;

    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE:
    case FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK:
    default:
        return FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL;
    }
}

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
        *dataArrayP = lwm2m_data_new(6);

        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;

        *numDataP = 6;
        (*dataArrayP)[0].id = FIRMWARE_RESOURCE_STATE;
        (*dataArrayP)[1].id = FIRMWARE_RESOURCE_UPDATE_RESULT;
        (*dataArrayP)[2].id = FIRMWARE_RESOURCE_PROTOCOL_SUPPORT;
        (*dataArrayP)[3].id = FIRMWARE_RESOURCE_DELIVERY_METHOD;
        (*dataArrayP)[4].id = FIRMWARE_RESOURCE_SEVERITY;
        (*dataArrayP)[5].id = FIRMWARE_RESOURCE_MAXIMUM_DEFER_PERIOD;
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
                instanceP->service->state,
                *dataArrayP + index
            );
            break;

        case FIRMWARE_RESOURCE_UPDATE_RESULT:
            lwm2m_data_encode_int(
                instanceP->service->update_result,
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

        case FIRMWARE_RESOURCE_SEVERITY:
            lwm2m_data_encode_int(
                instanceP->service->severity,
                *dataArrayP + index
            );
            break;

        case FIRMWARE_RESOURCE_MAXIMUM_DEFER_PERIOD:
            lwm2m_data_encode_uint(
                instanceP->service->maximum_defer_period_seconds,
                *dataArrayP + index
            );
            break;

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
            return COAP_501_NOT_IMPLEMENTED;
        case FIRMWARE_RESOURCE_PACKAGE_URI:
        {
            firmware_download_transport_status_t transport_status;

            if (instanceP->download_transport == NULL ||
                instanceP->download_transport->start == NULL)
                return COAP_501_NOT_IMPLEMENTED;

            if (dataArray[index].type != LWM2M_TYPE_STRING ||
                dataArray[index].value.asBuffer.buffer == NULL ||
                dataArray[index].value.asBuffer.length == 0)
                return COAP_400_BAD_REQUEST;

            transport_status =
                instanceP->download_transport->start(
                    instanceP->download_transport->context,
                    (const char *)dataArray[index].value.asBuffer.buffer,
                    dataArray[index].value.asBuffer.length,
                    instanceP->service
                );
            
            if (transport_status != FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK)
            {
                firmware_update_service_status_t service_status =
                    firmware_update_service_fail_download(
                        instanceP->service,
                        prv_map_download_failure(transport_status)
                    );

                if (service_status !=
                    FIRMWARE_UPDATE_SERVICE_STATUS_OK)
                    return prv_map_service_status(service_status);
            }

            return prv_map_download_transport_status(transport_status);
        }    

        case FIRMWARE_RESOURCE_SEVERITY:
        {
            int64_t severity;
            firmware_update_service_status_t service_status;

            if (!lwm2m_data_decode_int(
                    dataArray + index,
                    &severity))
                return COAP_400_BAD_REQUEST;

            service_status = firmware_update_service_set_severity(
                instanceP->service,
                (firmware_update_severity_t)severity
            );

            if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK)
                return prv_map_service_status(service_status);

            break;
        }

        case FIRMWARE_RESOURCE_MAXIMUM_DEFER_PERIOD:
        {
            uint64_t seconds;
            firmware_update_service_status_t service_status;

            if (!lwm2m_data_decode_uint(
                    dataArray + index,
                    &seconds))
                return COAP_400_BAD_REQUEST;

            service_status =
                firmware_update_service_set_maximum_defer_period(
                    instanceP->service,
                    seconds
                );

            if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK)
                return prv_map_service_status(service_status);

            break;
        }

        default:
            return COAP_405_METHOD_NOT_ALLOWED;
        }
    }

    return COAP_204_CHANGED;
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

    if (length != 0)
        return COAP_400_BAD_REQUEST;
    
    switch (resourceId)
    {
    case FIRMWARE_RESOURCE_UPDATE:
        return prv_map_service_status(
            firmware_update_service_install(instanceP->service)
        );

    case FIRMWARE_RESOURCE_CANCEL:
        firmware_download_transport_status_t transport_status;

        if (instanceP->service->state !=
                FIRMWARE_UPDATE_STATE_DOWNLOADING &&
            instanceP->service->state !=
                FIRMWARE_UPDATE_STATE_DOWNLOADED)
            return COAP_405_METHOD_NOT_ALLOWED;

        if (instanceP->download_transport != NULL)
        {
            if (instanceP->download_transport->cancel == NULL)
                return COAP_501_NOT_IMPLEMENTED;

            transport_status =
                instanceP->download_transport->cancel(
                    instanceP->download_transport->context
                );

            if (transport_status != FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK)
                return prv_map_download_transport_status(transport_status);
        }

        return prv_map_service_status(
            firmware_update_service_cancel(instanceP->service)
        );

    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

lwm2m_object_t *get_firmware_update_object(
    firmware_update_service_t *service,
    firmware_download_transport_t *download_transport)
{
    lwm2m_object_t *objectP;
    firmware_instance_t *instanceP;

    if (service == NULL)
        return NULL;

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
    instanceP->service = service;
    instanceP->download_transport = download_transport;

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
