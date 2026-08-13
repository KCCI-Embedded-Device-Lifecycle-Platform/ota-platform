package ota.platform.server.hawkbit;

public record HawkbitFirmwareAction(
        String endpoint,
        long actionId,
        long softwareModuleId,
        boolean installAfterDownload) {
}
