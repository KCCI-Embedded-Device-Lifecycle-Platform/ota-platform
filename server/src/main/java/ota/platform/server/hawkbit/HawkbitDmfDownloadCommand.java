package ota.platform.server.hawkbit;

import java.util.List;
import java.util.Map;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;

@JsonIgnoreProperties(ignoreUnknown = true)
public record HawkbitDmfDownloadCommand(
        long actionId,
        String targetSecurityToken,
        List<SoftwareModule> softwareModules) {

    @JsonIgnoreProperties(ignoreUnknown = true)
    public record SoftwareModule(
            long moduleId,
            String moduleType,
            String moduleVersion,
            List<Artifact> artifacts) {
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    public record Artifact(
            String filename,
            Map<String, String> urls,
            Hashes hashes,
            long size) {
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    public record Hashes(
            String md5,
            String sha1) {
    }
}
