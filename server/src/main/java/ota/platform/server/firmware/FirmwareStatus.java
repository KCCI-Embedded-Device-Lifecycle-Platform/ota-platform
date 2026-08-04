package ota.platform.server.firmware;

public record FirmwareStatus(
        String endpoint,
        int state,
        int updateResult) {

}
