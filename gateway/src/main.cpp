#include "object_bms.h"
#include "wakaama_hooks.h"
//#include <bits/stdc++.h>
#include <iostream>
#include <unistd.h>
using namespace std;

static bool testConnectionClose()
{

    gateway_client_context_t context {};

    lwm2m_connection_t *firstP =
        static_cast<lwm2m_connection_t *>(lwm2m_malloc(sizeof(lwm2m_connection_t)));

    lwm2m_connection_t *secondP =
        static_cast<lwm2m_connection_t *>(lwm2m_malloc(sizeof(lwm2m_connection_t)));

    if (firstP == nullptr || secondP == nullptr)
    {
        if (firstP != nullptr)
            lwm2m_free(firstP);

        if (secondP != nullptr)
            lwm2m_free(secondP);

        return false;
    }

    firstP->next = secondP;
    secondP->next = nullptr;
    context.connectionList = firstP;

    lwm2m_close_connection(secondP, &context);

    bool removedSecond =
        context.connectionList == firstP &&
        firstP->next == nullptr;

    lwm2m_close_connection(firstP, &context);

    bool removedFirst =
        context.connectionList == nullptr;

    if (context.connectionList != nullptr)
    {
        lwm2m_connection_free(context.connectionList);
        context.connectionList = nullptr;
    }

    return removedSecond && removedFirst;
}

int main()
{
    if(!testConnectionClose())
    {
        cerr << "Connection close test failed\n";
        return 1;
    }
    cout << "Connection close test passed\n";

    gateway_client_context_t clientContext{};

    clientContext.securityObjectP = nullptr;
    clientContext.socketFd = -1;
    clientContext.connectionList = nullptr;
    clientContext.addressFamily = AF_INET;
    clientContext.serverHost = "127.0.0.1";
    clientContext.serverPort = "5683";

    constexpr const char *clientPort = "56830";

    clientContext.socketFd = lwm2m_create_socket(clientPort, clientContext.addressFamily);

    if(clientContext.socketFd < 0)
    {
        cerr << "Failed to create UDP Socket\n";
        return 1;
    }

    cout << "UDP socket opened on port " << clientPort << '\n';

    void *sessionH = lwm2m_connect_server(0, &clientContext);

    if (sessionH == nullptr)
    {
        cerr << "Failed to create server connection\n";
        ::close(clientContext.socketFd);
        return 1;
    }

    cout  << "UDP connection created for "
          << clientContext.serverHost << ':'
          << clientContext.serverPort << '\n';

    lwm2m_close_connection(sessionH, &clientContext);

    if (clientContext.connectionList != nullptr)
    {
        cerr << "Failed to close server connection\n";

        lwm2m_connection_free(clientContext.connectionList);

        clientContext.connectionList = nullptr;

        ::close(clientContext.socketFd);
        return 1;
    }
    cout << "UDP connection closed\n";

    lwm2m_object_t *bmsObjectP = get_bms_object();

    if(bmsObjectP == nullptr){
        cerr << "Failed to create BMS Object\n";
        ::close(clientContext.socketFd);
        return 1;
    }
    int numData = 0;
    lwm2m_data_t *dataArrayP = nullptr;

    uint8_t readResult = bmsObjectP->readFunc(nullptr, 0, &numData, &dataArrayP, bmsObjectP);

    double voltage = 0.0;
    bool readSucceeded = readResult == COAP_205_CONTENT &&
                         numData == 1 &&
                        dataArrayP != nullptr &&
                        lwm2m_data_decode_float(&dataArrayP[0], &voltage) != 0;
    
    if(readSucceeded)
    {
        cout << "BMS object ID: "<< bmsObjectP->objID << '\n';
        cout << "BMS voltage: " << voltage << '\n';
    }
    else
    {
        cerr << "Failed to read BMS voltage\n";
    }

    if (dataArrayP != nullptr)
        lwm2m_data_free(numData, dataArrayP);

    free_bms_object(bmsObjectP);
    ::close(clientContext.socketFd);
    clientContext.socketFd = -1;
    
    return readSucceeded ? 0 : 1;
}