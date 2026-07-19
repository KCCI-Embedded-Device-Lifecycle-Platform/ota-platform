#include "gateway_app.hpp"
#include "object_bms.h"
#include "object_firmware.h"
#include "standard_objects.h"
#include <ctime>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

using namespace std;

namespace
{
    volatile std::sig_atomic_t stopRequested = 0;

    void requestStop(int)
    {
        stopRequested = 1;
    }
}

GatewayApp::GatewayApp() : clientContext{}, lwm2mContextP{nullptr}, objects{}
{
    clientContext.securityObjectP = nullptr;
    clientContext.socketFd = -1;
    clientContext.connectionList = nullptr;
    clientContext.addressFamily = AF_INET;
    clientContext.serverHost = "127.0.0.1";
    clientContext.serverPort = "5683";
}

GatewayApp::~GatewayApp()
{
    if (lwm2mContextP != nullptr)
    {
        lwm2m_close(lwm2mContextP);
        lwm2mContextP = nullptr;
    }

    if (clientContext.connectionList != nullptr)
    {
        lwm2m_connection_free(clientContext.connectionList);
        clientContext.connectionList = nullptr;
    }

    destroyClientObjects();

    if (clientContext.socketFd >= 0)
    {
        ::close(clientContext.socketFd);
        clientContext.socketFd = -1;
    }
}

bool GatewayApp::createClientObjects()
{
    constexpr int serverId = 123;
    constexpr int lifetime = 300;
    constexpr const char *serverUri = "coap://127.0.0.1:5683";
    constexpr const char *binding = "U";

    objects[SecurityObjectIndex] = get_security_object(serverId, serverUri, nullptr, nullptr, 0, false);
    objects[ServerObjectIndex] = get_server_object(serverId, binding, lifetime, false);
    objects[DeviceObjectIndex] = get_object_device();
    objects[FirmwareObjectIndex] = get_firmware_update_object();
    objects[BmsObjectIndex] = get_bms_object();

    if (objects[SecurityObjectIndex] == nullptr ||
        objects[ServerObjectIndex] == nullptr ||
        objects[DeviceObjectIndex] == nullptr ||
        objects[FirmwareObjectIndex] == nullptr ||
        objects[BmsObjectIndex] == nullptr)
    {
        cerr << "Failed to create client objects\n";
        destroyClientObjects();
        return false;
    }

    clientContext.securityObjectP = objects[SecurityObjectIndex];

    return true;
}

void GatewayApp::destroyClientObjects()
{
    clientContext.securityObjectP = nullptr;

    if (objects[BmsObjectIndex] != nullptr)
    {
        free_bms_object(objects[BmsObjectIndex]);
        objects[BmsObjectIndex] = nullptr;
    }

    if (objects[FirmwareObjectIndex] != nullptr)
    {
        free_firmware_update_object(objects[FirmwareObjectIndex]);
        objects[FirmwareObjectIndex] = nullptr;
    }

    if (objects[DeviceObjectIndex] != nullptr)
    {
        free_object_device(objects[DeviceObjectIndex]);
        objects[DeviceObjectIndex] = nullptr;
    }

    if (objects[ServerObjectIndex] != nullptr)
    {
        clean_server_object(objects[ServerObjectIndex]);
        lwm2m_free(objects[ServerObjectIndex]);
        objects[ServerObjectIndex] = nullptr;
    }

    if (objects[SecurityObjectIndex] != nullptr)
    {
        clean_security_object(objects[SecurityObjectIndex]);
        lwm2m_free(objects[SecurityObjectIndex]);
        objects[SecurityObjectIndex] = nullptr;
    }
}

bool GatewayApp::initialize()
{
    constexpr const char *clientPort = "56830";

    clientContext.socketFd = lwm2m_create_socket(clientPort, clientContext.addressFamily);

    if (clientContext.socketFd < 0)
    {
        cerr << "Failed to create LwM2M UDP socket\n";
        return false;
    }
    cout << "LwM2M UDP socket opened on port " << clientPort << '\n';

    if (!createClientObjects())
    {
        return false;
    }
    cout << "Client objects created: /0, /1, /3, /5 (wiring scaffold), /33000\n";

    lwm2mContextP = lwm2m_init(&clientContext);

    if (lwm2mContextP == nullptr)
    {
        cerr << "Failed to initialize Wakaama context\n";
        return false;
    }
    cout << "Wakaama context initialized\n";

    constexpr const char *endpointName = "gateway-01";

    int configureResult = lwm2m_configure(
        lwm2mContextP,
        endpointName,
        nullptr,
        nullptr,
        static_cast<uint16_t>(objectCount),
        objects.data()
    );

    if (configureResult != COAP_NO_ERROR)
    {
        cerr << "Failed to configure Wakaama: " << configureResult << '\n';

        lwm2m_close(lwm2mContextP);
        lwm2mContextP = nullptr;

        return false;
    }

    cout << "Wakaama configured as " << endpointName << '\n';

    return true;
}

int GatewayApp::run()
{
    if (lwm2mContextP == nullptr)
    {
        cerr << "Wakaama context is not initialized\n";
        return 1;
    }

    stopRequested = 0;

    struct sigaction signalAction{};
    signalAction.sa_handler = requestStop;
    ::sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;

    if (::sigaction(SIGINT, &signalAction, nullptr) < 0 ||
        ::sigaction(SIGTERM, &signalAction, nullptr) < 0)
    {
        cerr << "Failed to install shutdown signal handler\n";
        return 1;
    }

    auto previousState = lwm2mContextP->state;

    while (stopRequested == 0)
    {
        std::time_t timeoutSeconds = 60;
        int stepResult = lwm2m_step(lwm2mContextP, &timeoutSeconds);

        if (stepResult != COAP_NO_ERROR)
        {
            cerr << "lwm2m_step failed: " << stepResult << '\n';
            return 1;
        }

        if (lwm2mContextP->state != previousState)
        {
            if (lwm2mContextP->state == STATE_REGISTERING)
            {
                cout << "Wakaama state: REGISTERING\n";
            }
            else if (lwm2mContextP->state == STATE_READY)
            {
                cout << "Wakaama state: READY\n";
            }
            else
            {
                cout << "Wakaama state changed: " << lwm2mContextP->state << '\n';
            }

            previousState = lwm2mContextP->state;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(clientContext.socketFd, &readSet);

        timeval waitTime{};
        waitTime.tv_sec = timeoutSeconds;

        int selectResult = ::select(
            clientContext.socketFd + 1,
            &readSet,
            nullptr,
            nullptr,
            &waitTime
        );

        if (selectResult < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            cerr << "Failed to wait for UDP packet\n";
            return 1;
        }

        if (selectResult == 0)
        {
            continue;
        }

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
            if (errno == EINTR)
            {
                continue;
            }

            cerr << "Failed to receive UDP packet\n";
            return 1;
        }

        if (receivedBytes == 0)
        {
            cerr << "Ignored an empty UDP packet\n";
            continue;
        }

        lwm2m_connection_t *connectionP = lwm2m_connection_find(
            clientContext.connectionList,
            &senderAddress,
            senderAddressLength
        );

        if (connectionP == nullptr)
        {
            cerr << "Ignored UDP packet from an unknown sender\n";
            continue;
        }

        lwm2m_handle_packet(
            lwm2mContextP,
            receiveBuffer,
            static_cast<std::size_t>(receivedBytes),
            connectionP
        );
    }

    cout << "Gateway shutdown requested\n";

    return 0;
}
