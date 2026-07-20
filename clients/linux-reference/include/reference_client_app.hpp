#ifndef OTA_LINUX_REFERENCE_CLIENT_APP_HPP
#define OTA_LINUX_REFERENCE_CLIENT_APP_HPP

#include "wakaama_hooks.h"

#include <array>
#include <cstddef>

class ReferenceClientApp
{
public:
    ReferenceClientApp();
    ~ReferenceClientApp();

    bool initialize();
    int run();

private:
    bool createClientObjects();
    void destroyClientObjects();

    static constexpr std::size_t SecurityObjectIndex = 0;
    static constexpr std::size_t ServerObjectIndex = 1;
    static constexpr std::size_t DeviceObjectIndex = 2;
    static constexpr std::size_t FirmwareObjectIndex = 3;
    static constexpr std::size_t BmsObjectIndex = 4;
    static constexpr std::size_t objectCount = 5;

    wakaama_client_context_t clientContext;
    lwm2m_context_t *lwm2mContextP;
    std::array<lwm2m_object_t *, objectCount> objects;
};

#endif
