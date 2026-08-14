package ota.platform.server.ui;

import java.util.UUID;

public record DashboardDevice(
        UUID id,
        String endpoint,
        String displayName,
        boolean enabled,
        boolean online) {
}
