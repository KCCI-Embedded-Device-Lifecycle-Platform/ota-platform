package ota.platform.server.device;

public record CreateDeviceRequest(
        String endpoint,
        String displayName) {
}