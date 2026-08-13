package ota.platform.server.hawkbit;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Map;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

@Service
public class HawkbitArtifactStagingService {

    private final Path cacheDirectory;
    private final long maxSizeBytes;
    private final HttpClient httpClient;

    public HawkbitArtifactStagingService(
            @Value("${ota.hawkbit.artifact-cache-directory}")
            String cacheDirectory,
            @Value("${ota.hawkbit.artifact-max-size-bytes}")
            long maxSizeBytes) {

        if (maxSizeBytes <= 0) {
            throw new IllegalArgumentException(
                    "artifact max size must be positive");
        }

        this.cacheDirectory =
                Path.of(cacheDirectory).toAbsolutePath().normalize();
        this.maxSizeBytes = maxSizeBytes;
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(10))
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
    }

    public StagedHawkbitArtifact stage(
            long actionId,
            long softwareModuleId,
            String targetSecurityToken,
            HawkbitDmfDownloadCommand.Artifact artifact)
            throws IOException, InterruptedException {

        validateArtifact(artifact);

        if (targetSecurityToken == null
                || !targetSecurityToken.matches("[A-Za-z0-9]{1,128}")) {
            throw new IOException(
                    "target security token is invalid");
        }

        URI downloadUri = selectDownloadUri(artifact);
        Path targetPath =
                resolveStagingPath(actionId, softwareModuleId);
        Path partialPath = targetPath.resolveSibling(
                targetPath.getFileName() + ".part");

        Files.createDirectories(targetPath.getParent());

        HttpRequest request = HttpRequest.newBuilder(downloadUri)
                .timeout(Duration.ofMinutes(2))
                .header(
                        "Authorization",
                        "TargetToken " + targetSecurityToken)
                .GET()
                .build();

        HttpResponse<InputStream> response = httpClient.send(
                request,
                HttpResponse.BodyHandlers.ofInputStream());

        try (InputStream input = response.body()) {
            if (response.statusCode() < 200
                    || response.statusCode() >= 300) {
                throw new IOException(
                        "artifact download failed with HTTP "
                                + response.statusCode());
            }

            DownloadedContent downloaded =
                    writePartialFile(input, partialPath);

            if (downloaded.size() != artifact.size()) {
                throw new IOException(
                        "artifact size does not match metadata");
            }

            if (!downloaded.sha1().equalsIgnoreCase(
                    artifact.hashes().sha1())) {
                throw new IOException(
                        "artifact SHA-1 does not match metadata");
            }

            moveCompletedFile(partialPath, targetPath);

            return new StagedHawkbitArtifact(
                    actionId,
                    softwareModuleId,
                    artifact.filename(),
                    targetPath,
                    downloaded.size(),
                    downloaded.sha1());
        } finally {
            Files.deleteIfExists(partialPath);
        }
    }

    private URI selectDownloadUri(
            HawkbitDmfDownloadCommand.Artifact artifact)
            throws IOException {

        Map<String, String> urls = artifact.urls();

        if (urls == null) {
            throw new IOException("artifact URL is missing");
        }

        String rawUri = urls.get("HTTPS");

        if (rawUri == null || rawUri.isBlank()) {
            rawUri = urls.get("HTTP");
        }

        if (rawUri == null || rawUri.isBlank()) {
            throw new IOException(
                    "artifact has no HTTP or HTTPS URL");
        }

        URI uri;

        try {
            uri = URI.create(rawUri);
        } catch (IllegalArgumentException error) {
            throw new IOException("artifact URL is invalid", error);
        }

        String scheme = uri.getScheme();

        if (!"http".equalsIgnoreCase(scheme)
                && !"https".equalsIgnoreCase(scheme)) {
            throw new IOException(
                    "unsupported artifact URL scheme: " + scheme);
        }

        return uri;
    }

    private void validateArtifact(
            HawkbitDmfDownloadCommand.Artifact artifact)
            throws IOException {

        if (artifact == null) {
            throw new IOException("artifact is missing");
        }

        if (artifact.filename() == null
                || artifact.filename().isBlank()) {
            throw new IOException("artifact filename is missing");
        }

        if (artifact.size() <= 0) {
            throw new IOException("artifact size is invalid");
        }

        if (artifact.size() > maxSizeBytes) {
            throw new IOException(
                    "artifact exceeds configured maximum size");
        }

        if (artifact.hashes() == null
                || artifact.hashes().sha1() == null
                || !artifact.hashes().sha1()
                        .matches("(?i)[0-9a-f]{40}")) {
            throw new IOException("artifact SHA-1 is invalid");
        }
    }

    private Path resolveStagingPath(
            long actionId,
            long softwareModuleId)
            throws IOException {

        if (actionId <= 0 || softwareModuleId <= 0) {
            throw new IOException(
                    "actionId or softwareModuleId is invalid");
        }

        Path path = cacheDirectory
                .resolve(Long.toString(actionId))
                .resolve(softwareModuleId + ".bin")
                .normalize();

        if (!path.startsWith(cacheDirectory)) {
            throw new IOException(
                    "artifact staging path escaped cache directory");
        }

        return path;
    }

    private DownloadedContent writePartialFile(
            InputStream input,
            Path partialPath)
            throws IOException {

        MessageDigest digest = newSha1Digest();
        byte[] buffer = new byte[8192];
        long totalSize = 0;

        try (OutputStream output = Files.newOutputStream(
                partialPath,
                StandardOpenOption.CREATE,
                StandardOpenOption.TRUNCATE_EXISTING,
                StandardOpenOption.WRITE)) {

            int read;

            while ((read = input.read(buffer)) >= 0) {
                if (read == 0) {
                    continue;
                }

                totalSize += read;

                if (totalSize > maxSizeBytes) {
                    throw new IOException(
                            "artifact exceeded maximum size");
                }

                digest.update(buffer, 0, read);
                output.write(buffer, 0, read);
            }
        }

        return new DownloadedContent(
                totalSize,
                HexFormat.of().formatHex(digest.digest()));
    }

    private void moveCompletedFile(
            Path partialPath,
            Path targetPath)
            throws IOException {

        try {
            Files.move(
                    partialPath,
                    targetPath,
                    StandardCopyOption.ATOMIC_MOVE,
                    StandardCopyOption.REPLACE_EXISTING);
        } catch (AtomicMoveNotSupportedException error) {
            Files.move(
                    partialPath,
                    targetPath,
                    StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private MessageDigest newSha1Digest() {
        try {
            return MessageDigest.getInstance("SHA-1");
        } catch (NoSuchAlgorithmException error) {
            throw new IllegalStateException(
                    "SHA-1 algorithm is unavailable",
                    error);
        }
    }

    private record DownloadedContent(long size, String sha1) {
    }    

}
