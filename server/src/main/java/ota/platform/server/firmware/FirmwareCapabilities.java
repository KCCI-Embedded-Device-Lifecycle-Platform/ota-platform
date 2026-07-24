package ota.platform.server.firmware;

import java.util.List;

public record FirmwareCapabilities (
    String endpoint,
    List<Integer> protocolSupport,
    int deliveryMethod) {
}
