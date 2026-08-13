package ota.platform.server.hawkbit;

import java.nio.file.Path;

public record StagedHawkbitArtifact(
        long actionId,
        long softwareModuleId,
        String filename,
        Path path,
        long size,
        String sha1) {
}
