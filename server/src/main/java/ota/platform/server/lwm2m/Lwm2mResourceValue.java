package ota.platform.server.lwm2m;

public record Lwm2mResourceValue(
        String endpoint,
        String path,
        boolean multiple,
        Object value) {
}
