#include "wakaama_hooks.h"
int g_reboot = 0;

void *lwm2m_connect_server(uint16_t securityInstanceId, void *userData)
{
    wakaama_client_context_t *contextP;
    lwm2m_dtls_connection_t *connectionP;

    contextP = (wakaama_client_context_t *)userData;

    if (contextP == NULL)
        return NULL;

    if (contextP->socketFd < 0 ||
    contextP->securityObjectP == NULL ||
    contextP->lwm2mContextP == NULL)
    {
        return NULL;
    }

    connectionP = lwm2m_connection_create(
        contextP->connectionList,
        contextP->socketFd,
        contextP->securityObjectP,
        securityInstanceId,
        contextP->lwm2mContextP,
        contextP->addressFamily
    );

    if (connectionP == NULL)
        return NULL;

    contextP->connectionList = connectionP;

    return connectionP;
}

void lwm2m_close_connection(void *sessionH, void *userData)
{
    wakaama_client_context_t *contextP;
    lwm2m_dtls_connection_t *targetP;
    //lwm2m_dtls_connection_t *parentP;

    contextP = (wakaama_client_context_t *)userData;
    targetP = (lwm2m_dtls_connection_t *)sessionH;

    if (contextP == NULL || targetP == NULL)
        return;

    if (targetP == contextP->connectionList)
    {
        contextP->connectionList = targetP->next;
    }
    else
    {
        lwm2m_dtls_connection_t *parentP = contextP->connectionList;

        while (parentP != NULL &&
               parentP->next != targetP)
        {
            parentP = parentP->next;
        }

        if (parentP == NULL)
            return;

        parentP->next = targetP->next;
    }

    /*
     * tinyDTLS callbacks must not retain a pointer
     * to the connection that is about to be freed.
     */
    lwm2m_connection_set_list(contextP->connectionList);

    if (targetP->dtlsSession != NULL)
    {
        lwm2m_free(targetP->dtlsSession);
        targetP->dtlsSession = NULL;
    }
    lwm2m_free(targetP);
}
