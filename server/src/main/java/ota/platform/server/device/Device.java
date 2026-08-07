package ota.platform.server.device;

import java.time.Instant;
import java.util.UUID;

public record Device(
        UUID id,
        String endpoint,
        String displayName,
        boolean enabled,
        Instant createdAt,
        Instant updatedAt) {
}
