#include "object_bms.h"
#include "standard_objects.h"
#include "wakaama_hooks.h"
//#include <bits/stdc++.h>
#include <iostream>
#include <ctime>
#include <sys/select.h>
#include <unistd.h>
using namespace std;

constexpr uint16_t SECURITY_OBJECT_INDEX = 0;
constexpr uint16_t SERVER_OBJECT_INDEX = 1;
constexpr uint16_t DEVICE_OBJECT_INDEX = 2;
constexpr uint16_t BMS_OBJECT_INDEX = 3;
constexpr uint16_t CLIENT_OBJECT_COUNT = 4;

static bool testSecurityObject()
{
    constexpr int serverId = 123;
    constexpr const char *serverUri = "coap://127.0.0.1:5683";

    lwm2m_object_t *securityObjectP = get_security_object(
        serverId,
        serverUri,
        nullptr,
        nullptr,
        0,
        false
    );

    if (securityObjectP == nullptr)
        return false;
    
    bool validObject = securityObjectP->objID == 0;

    clean_security_object(securityObjectP);
    lwm2m_free(securityObjectP);

    return validObject;

}

static bool testServerObject()
{
    constexpr int serverId = 123;
    constexpr int lifetime = 300;
    constexpr const char *binding = "U";

    lwm2m_object_t *serverObjectP = get_server_object(serverId, binding, lifetime, false);

    if (serverObjectP == nullptr)
        return false;

    bool validObject = serverObjectP->objID == 1;

    clean_server_object(serverObjectP);
    lwm2m_free(serverObjectP);

    return validObject;
}

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

static void freeClientObjects(lwm2m_object_t *objects[])
{
    if (objects[DEVICE_OBJECT_INDEX] != nullptr)
    {
        free_object_device(objects[DEVICE_OBJECT_INDEX]);
        objects[DEVICE_OBJECT_INDEX] = nullptr;
    }

    if (objects[BMS_OBJECT_INDEX] != nullptr)
    {
        free_bms_object(objects[BMS_OBJECT_INDEX]);
        objects[BMS_OBJECT_INDEX] = nullptr;
    }

    if (objects[SERVER_OBJECT_INDEX] != nullptr)
    {
        clean_server_object(objects[SERVER_OBJECT_INDEX]);
        lwm2m_free(objects[SERVER_OBJECT_INDEX]);
        objects[SERVER_OBJECT_INDEX] = nullptr;
    }

    if (objects[SECURITY_OBJECT_INDEX] != nullptr)
    {
        clean_security_object(objects[SECURITY_OBJECT_INDEX]);
        lwm2m_free(objects[SECURITY_OBJECT_INDEX]);
        objects[SECURITY_OBJECT_INDEX] = nullptr;
    }

}

int main()
{
    if(!testConnectionClose())
    {
        cerr << "Connection close test failed\n";
        return 1;
    }
    cout << "Connection close test passed\n";

    if (!testSecurityObject())
    {
        cerr << "Security object test failed\n";
        return 1;
    }
    cout << "Security object test passed\n";

    if (!testServerObject())
    {
        cerr << "Server object test failed\n";
        return 1;
    }
    cout << "Server object test passed\n";

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

    constexpr int serverId = 123;
    constexpr int lifetime = 300;
    constexpr const char *serverUri = "coap://127.0.0.1:5683";

    lwm2m_object_t *objects[CLIENT_OBJECT_COUNT]{};

    objects[SECURITY_OBJECT_INDEX] = get_security_object(serverId, serverUri, nullptr, nullptr, 0, false);
    objects[SERVER_OBJECT_INDEX] = get_server_object(serverId, "U", lifetime, false);
    objects[DEVICE_OBJECT_INDEX] = get_object_device();
    objects[BMS_OBJECT_INDEX] = get_bms_object();

    if (objects[SECURITY_OBJECT_INDEX] == nullptr ||
        objects[SERVER_OBJECT_INDEX] == nullptr ||
        objects[DEVICE_OBJECT_INDEX] == nullptr ||
        objects[BMS_OBJECT_INDEX] == nullptr)
    {
        cerr << "Failed to create client objects\n";

        freeClientObjects(objects);
        ::close(clientContext.socketFd);

        return 1;
    }

    lwm2m_context_t *lwm2mContextP = lwm2m_init(&clientContext);
    if (lwm2mContextP == nullptr)
    {
        cerr << "Failed to initialize Wakaama context\n";

        freeClientObjects(objects);
        clientContext.securityObjectP = nullptr;

        ::close(clientContext.socketFd);
        return 1;
    }
    cout << "Wakaama context initialized\n";

    clientContext.securityObjectP = objects[SECURITY_OBJECT_INDEX];
    lwm2m_object_t *bmsObjectP = objects[BMS_OBJECT_INDEX];

    constexpr const char *endpointName = "gateway-01";

    int configureResult = lwm2m_configure(
        lwm2mContextP,
        endpointName,
        nullptr,
        nullptr,
        CLIENT_OBJECT_COUNT,
        objects
    );

    if(configureResult != COAP_NO_ERROR)
    {
        std::cerr << "Failed to configure Wakaama: "
              << configureResult
              << '\n';

        lwm2m_close(lwm2mContextP);

        freeClientObjects(objects);
        clientContext.securityObjectP = nullptr;

        ::close(clientContext.socketFd);
        return 1;
    }
    cout << "Wakaama configured as " << endpointName << '\n';

    cout << "Client object array created: "
         << "/0, /1, /3, /33000\n";

    time_t timeoutSeconds = 60;

    int stepResult = lwm2m_step(
        lwm2mContextP,
        &timeoutSeconds
    );

    if (stepResult != COAP_NO_ERROR)
    {
        cerr << "lwm2m_step failed: " << stepResult << '\n';
        lwm2m_close(lwm2mContextP);

        if (clientContext.connectionList != nullptr)
        {
            lwm2m_connection_free(clientContext.connectionList);
            clientContext.connectionList = nullptr;
        }

        freeClientObjects(objects);
        clientContext.securityObjectP = nullptr;

        ::close(clientContext.socketFd);
        return 1;
    }

    if (lwm2mContextP->state == STATE_REGISTERING)
    {
        cout << "Wakaama state: REGISTERING\n";
    }
    else
    {
        cout << "Unexpected Wakaama state: " << lwm2mContextP->state << '\n';
    }
    
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientContext.socketFd, &readSet);

    timeval waitTime{};
    waitTime.tv_sec = 5;
    waitTime.tv_usec = 0;

    int selectResult = select(
        clientContext.socketFd + 1,
        &readSet,
        nullptr,
        nullptr,
        &waitTime
    );

    if (selectResult < 0)
    {
        std::cerr << "Failed to wait for UDP response\n";
    }
    else if (selectResult == 0)
    {
        std::cerr << "Registration response timed out\n";
    }
    else if (FD_ISSET(clientContext.socketFd, &readSet))
    {
        uint8_t receiveBuffer[LWM2M_COAP_MAX_MESSAGE_SIZE];

        sockaddr_storage senderAddress{};

        socklen_t senderAddressLength = sizeof(senderAddress);

        ssize_t receivedBytes = ::recvfrom(
            clientContext.socketFd,
            receiveBuffer,
            sizeof(receiveBuffer),
            0,
            reinterpret_cast<sockaddr *>(&senderAddress),
            &senderAddressLength
        );

        if (receivedBytes < 0)
        {
            cerr << "Failed to receive UDP response\n";
        }
        else if (receivedBytes == 0)
        {
            cerr << "Received an empty UDP response\n";
        }
        else if (senderAddress.ss_family == AF_INET)
        {
            auto *ipv4Address =
                reinterpret_cast<sockaddr_in *>(&senderAddress);

            char senderIp[INET_ADDRSTRLEN]{};

            const char *convertedAddress = inet_ntop(
                AF_INET,
                &ipv4Address->sin_addr,
                senderIp,
                sizeof(senderIp)
            );

            if (convertedAddress == nullptr)
            {
                cerr << "Failed to convert sender address\n";
            }
            else
            {
                cout << "Registration response: "
                     << receivedBytes
                     << " bytes from "
                     << senderIp
                     << ':'
                     << ntohs(ipv4Address->sin_port)
                     << '\n';
            }
        }
        else
        {
            std::cerr << "Received response from an unsupported address family\n";
        }
    }
    
    
    int numData = 0;
    lwm2m_data_t *dataArrayP = nullptr;

    uint8_t readResult = bmsObjectP->readFunc(lwm2mContextP, 0, &numData, &dataArrayP, bmsObjectP);

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

    lwm2m_close(lwm2mContextP);

    if (clientContext.connectionList != nullptr)
    {
        lwm2m_connection_free(clientContext.connectionList);
        clientContext.connectionList = nullptr;
    }

    freeClientObjects(objects);
    clientContext.securityObjectP = nullptr;

    ::close(clientContext.socketFd);
    clientContext.socketFd = -1;
    
    return readSucceeded ? 0 : 1;
}