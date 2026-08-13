package ota.platform.server.firmware;

public record FirmwareCommandResult(
        Status status,
        String detail) {

    public enum Status {
        ACCEPTED,
        DEVICE_OFFLINE,
        REJECTED
    }

    public boolean accepted() {
        return status == Status.ACCEPTED;
    }

    public static FirmwareCommandResult acceptedResult() {
        return new FirmwareCommandResult(Status.ACCEPTED, "");
    }

    public static FirmwareCommandResult deviceOffline() {
        return new FirmwareCommandResult(
                Status.DEVICE_OFFLINE,
                "device is not registered");
    }

    public static FirmwareCommandResult rejected(String detail) {
        return new FirmwareCommandResult(Status.REJECTED, detail);
    }
}
