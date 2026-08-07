#include "reference_client_app.hpp"
#include "object_bms.h"
#include "object_firmware.h"
#include "standard_objects.h"

#include <ctime>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <limits>

using namespace std;

namespace
{
    volatile std::sig_atomic_t stopRequested = 0;

    void requestStop(int)
    {
        stopRequested = 1;
    }

    int hexDigitValue(char value)
    {
        if (value >= '0' && value <= '9')
            return value - '0';

        if (value >= 'a' && value <= 'f')
            return value - 'a' + 10;

        if (value >= 'A' && value <= 'F')
            return value - 'A' + 10;

        return -1;
    }

    bool parseHexKey(
        const char *keyHex,
        std::vector<std::uint8_t> &key)
    {
        key.clear();

        if (keyHex == nullptr)
            return false;

        std::size_t length = std::strlen(keyHex);

        if (length == 0 || length % 2 != 0)
            return false;

        key.reserve(length / 2);

        for (std::size_t index = 0;
            index < length;
            index += 2)
        {
            int high = hexDigitValue(keyHex[index]);
            int low = hexDigitValue(keyHex[index + 1]);

            if (high < 0 || low < 0)
            {
                key.clear();
                return false;
            }

            key.push_back(
                static_cast<std::uint8_t>(
                    (high << 4) | low
                )
            );
        }

        return true;
    }
}

ReferenceClientApp::ReferenceClientApp() :
    firmwareBackendContext{}, firmwareBackend{}, firmwareUpdateService{},
    firmwareDownloadTransport{}, firmwareDownloadTransportContext{nullptr},
    lastNotifiedFirmwareState{FIRMWARE_UPDATE_STATE_IDLE}, lastNotifiedFirmwareResult{FIRMWARE_UPDATE_RESULT_INITIAL},
    clientContext{}, lwm2mContextP{nullptr}, objects{}
{
    clientContext.securityObjectP = nullptr;
    clientContext.socketFd = -1;
    clientContext.connectionList = nullptr;
    clientContext.addressFamily = AF_INET;
}

ReferenceClientApp::~ReferenceClientApp()
{
    if (lwm2mContextP != nullptr)
    {
        lwm2m_close(lwm2mContextP);
        lwm2mContextP = nullptr;
        clientContext.lwm2mContextP = nullptr;
    }

    lwm2m_connection_free(clientContext.connectionList);
    clientContext.connectionList = nullptr;

    destroyClientObjects();
    if (firmwareDownloadTransportContext != nullptr)
    {
        linux_coap_download_transport_destroy(
            firmwareDownloadTransportContext
        );
        firmwareDownloadTransportContext = nullptr;
    }
    linux_firmware_update_backend_deinit(&firmwareBackendContext);

    if (clientContext.socketFd >= 0)
    {
        ::close(clientContext.socketFd);
        clientContext.socketFd = -1;
    }
}

bool ReferenceClientApp::createClientObjects()
{
    constexpr int serverId = 123;
    constexpr int lifetime = 300;
    constexpr const char *serverUri = "coaps://127.0.0.1:5684";
    constexpr const char *binding = "U";

    char *pskIdentity = std::getenv("OTA_LWM2M_PSK_IDENTITY");
    const char *pskKeyHex = std::getenv("OTA_LWM2M_PSK_KEY_HEX");

    if (pskIdentity == nullptr || pskIdentity[0] == '\0' ||
        pskKeyHex == nullptr || pskKeyHex[0] == '\0')
    {
        cerr << "DTLS-PSK environment is incomplete\n";
        return false;
    }

    std::vector<std::uint8_t> pskKey;

    if (!parseHexKey(pskKeyHex, pskKey) ||
        pskKey.size() >
            std::numeric_limits<std::uint16_t>::max())
    {
        cerr << "DTLS-PSK key is invalid\n";
        return false;
    }

    objects[SecurityObjectIndex] = get_security_object(serverId, serverUri, pskIdentity, reinterpret_cast<char *>(pskKey.data()), static_cast<std::uint16_t>(pskKey.size()), false);
    objects[ServerObjectIndex] = get_server_object(serverId, binding, lifetime, false);
    objects[DeviceObjectIndex] = get_object_device();
    objects[FirmwareObjectIndex] = get_firmware_update_object(&firmwareUpdateService, &firmwareDownloadTransport);
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

void ReferenceClientApp::destroyClientObjects()
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

void ReferenceClientApp::notifyFirmwareResourceChanges()
{
    constexpr uint16_t firmwareObjectId = 5;
    constexpr uint16_t firmwareInstanceId = 0;
    lwm2m_uri_t resourceUri;

    if (lwm2mContextP == nullptr)
        return;

    if (firmwareUpdateService.state !=
        lastNotifiedFirmwareState)
    {
        LWM2M_URI_RESET(&resourceUri);
        resourceUri.objectId = firmwareObjectId;
        resourceUri.instanceId = firmwareInstanceId;
        resourceUri.resourceId = 3;

        lwm2m_resource_value_changed(
            lwm2mContextP,
            &resourceUri
        );

        lastNotifiedFirmwareState =
            firmwareUpdateService.state;
    }

    if (firmwareUpdateService.update_result !=
        lastNotifiedFirmwareResult)
    {
        LWM2M_URI_RESET(&resourceUri);
        resourceUri.objectId = firmwareObjectId;
        resourceUri.instanceId = firmwareInstanceId;
        resourceUri.resourceId = 5;

        lwm2m_resource_value_changed(
            lwm2mContextP,
            &resourceUri
        );

        lastNotifiedFirmwareResult =
            firmwareUpdateService.update_result;
    }
}

bool ReferenceClientApp::initialize()
{
    constexpr const char *firmwareStagingPath =
        "/tmp/ota-linux-reference-firmware.bin";

    constexpr const char *firmwareInstallMarkerPath =
    "/tmp/ota-linux-reference-install.pending";

    if (!linux_firmware_update_backend_init(
            &firmwareBackendContext,
            firmwareStagingPath,
            firmwareInstallMarkerPath,
            &firmwareBackend))
    {
        cerr << "Failed to initialize Linux firmware Backend\n";
        return false;
    }

    if (!firmware_update_service_init(
            &firmwareUpdateService,
            &firmwareBackend))
    {
        cerr << "Failed to initialize Firmware Update Service\n";
        return false;
    }

    if (firmware_update_service_recover_after_boot(
            &firmwareUpdateService) !=
        FIRMWARE_UPDATE_SERVICE_STATUS_OK)
    {
        cerr << "Failed to recover firmware update state\n";
        return false;
    }

    lastNotifiedFirmwareState = firmwareUpdateService.state;
    lastNotifiedFirmwareResult = firmwareUpdateService.update_result;

    firmwareDownloadTransportContext =
        linux_coap_download_transport_create(
            &firmwareDownloadTransport
        );

    if (firmwareDownloadTransportContext == nullptr)
    {
        cerr << "Failed to initialize Linux CoAP Download Transport\n";
        return false;
    }

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
    
    clientContext.lwm2mContextP = lwm2mContextP;
    constexpr const char *endpointName = "linux-reference-01";

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

int ReferenceClientApp::run()
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
        notifyFirmwareResourceChanges();
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

        int downloadSocketFd =
            linux_coap_download_transport_get_socket_fd(
                firmwareDownloadTransportContext
            );

        int downloadTimeoutMs =
            linux_coap_download_transport_get_timeout_ms(
                firmwareDownloadTransportContext
            );

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(clientContext.socketFd, &readSet);

        int maxSocketFd = clientContext.socketFd;

        if (downloadSocketFd >= 0)
        {
            FD_SET(downloadSocketFd, &readSet);

            if (downloadSocketFd > maxSocketFd)
                maxSocketFd = downloadSocketFd;
        }

        long waitMilliseconds =
            static_cast<long>(timeoutSeconds) * 1000L;

        if (waitMilliseconds < 0)
            waitMilliseconds = 0;

        if (downloadTimeoutMs >= 0 &&
            downloadTimeoutMs < waitMilliseconds)
        {
            waitMilliseconds = downloadTimeoutMs;
        }

        timeval waitTime{};
        waitTime.tv_sec = waitMilliseconds / 1000L;
        waitTime.tv_usec =
            (waitMilliseconds % 1000L) * 1000L;

        int selectResult = ::select(
            maxSocketFd + 1,
            &readSet,
            nullptr,
            nullptr,
            &waitTime
        );

        if (selectResult < 0)
        {
            if (errno == EINTR)
                continue;

            cerr << "Failed to wait for network event\n";
            return 1;
        }

        bool downloadSocketReadable =
            downloadSocketFd >= 0 &&
            FD_ISSET(downloadSocketFd, &readSet);

        /*
        * An active Transport is processed on either a socket event
        * or its periodic timeout.
        */
        if (downloadTimeoutMs >= 0 ||
            downloadSocketReadable)
        {
            firmware_download_transport_status_t status =
                linux_coap_download_transport_process(
                    firmwareDownloadTransportContext,
                    downloadSocketReadable
                );

            if (status !=
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK)
            {
                cerr << "Firmware download transport failed: "
                    << status << '\n';
            }
        }

        if (selectResult == 0)
            continue;

        /*
        * select() may have returned only for the libcoap fd.
        * Do not call recvfrom() unless the LwM2M socket is readable.
        */
        if (!FD_ISSET(clientContext.socketFd, &readSet))
            continue;

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

        lwm2m_dtls_connection_t *connectionP = lwm2m_connection_find(
            clientContext.connectionList,
            &senderAddress,
            senderAddressLength
        );

        if (connectionP == nullptr)
        {
            cerr << "Ignored UDP packet from an unknown sender\n";
            continue;
        }

        int handleResult =
            lwm2m_connection_handle_packet(
                connectionP,
                receiveBuffer,
                static_cast<std::size_t>(receivedBytes)
            );

        if (handleResult != 0)
        {
            cerr << "Failed to handle DTLS packet: "
                << handleResult << '\n';
        }
    }

    cout << "Reference client shutdown requested\n";

    return 0;
}
