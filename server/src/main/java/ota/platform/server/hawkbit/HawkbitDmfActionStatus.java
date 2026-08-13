package ota.platform.server.hawkbit;

import java.util.List;

public record HawkbitDmfActionStatus(
        long actionId,
        long softwareModuleId,
        Status actionStatus,
        List<String> message,
        long timestamp) {

    public enum Status {
        DOWNLOAD,
        DOWNLOADED,
        RUNNING,
        FINISHED,
        WARNING,
        ERROR,
        CANCELED,
        CANCEL_REJECTED
    }
}