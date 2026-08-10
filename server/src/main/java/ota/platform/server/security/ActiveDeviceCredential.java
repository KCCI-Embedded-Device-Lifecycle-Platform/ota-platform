package ota.platform.server.security;

import java.util.UUID;

public record ActiveDeviceCredential(
        UUID credentialId,
        UUID deviceId,
        String endpoint,
        String identity,
        String secretReference) {
}
