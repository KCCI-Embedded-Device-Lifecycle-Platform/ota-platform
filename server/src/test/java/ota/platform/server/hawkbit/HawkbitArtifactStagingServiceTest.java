package ota.platform.server.hawkbit;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import com.sun.net.httpserver.HttpServer;

class HawkbitArtifactStagingServiceTest {

    @TempDir
    Path temporaryDirectory;

    private HttpServer httpServer;

    @AfterEach
    void stopHttpServer() {
        if (httpServer != null) {
            httpServer.stop(0);
        }
    }

    @Test
    void stagesDownloadedArtifactAfterVerification()
            throws Exception {

        byte[] firmware =
                "test-firmware-image".getBytes(StandardCharsets.UTF_8);
        AtomicReference<String> authorization =
                new AtomicReference<>();

        httpServer = HttpServer.create(
                new InetSocketAddress("127.0.0.1", 0),
                0);

        httpServer.createContext("/artifact.bin", exchange -> {
            authorization.set(
                    exchange.getRequestHeaders()
                            .getFirst("Authorization"));

            exchange.sendResponseHeaders(200, firmware.length);

            try (var responseBody = exchange.getResponseBody()) {
                responseBody.write(firmware);
            }
        });

        httpServer.start();

        String downloadUrl = "http://127.0.0.1:"
                + httpServer.getAddress().getPort()
                + "/artifact.bin";

        var artifact = new HawkbitDmfDownloadCommand.Artifact(
                "firmware.bin",
                Map.of("HTTP", downloadUrl),
                new HawkbitDmfDownloadCommand.Hashes(
                        "unused",
                        sha1(firmware)),
                firmware.length);

        var service = new HawkbitArtifactStagingService(
                temporaryDirectory.toString(),
                1024);

        StagedHawkbitArtifact staged = service.stage(
                11,
                22,
                "testToken123",
                artifact);

        assertEquals(
                "TargetToken testToken123",
                authorization.get());
        assertEquals(firmware.length, staged.size());
        assertEquals(sha1(firmware), staged.sha1());
        assertArrayEquals(
                firmware,
                Files.readAllBytes(staged.path()));
        assertFalse(Files.exists(
                staged.path().resolveSibling("22.bin.part")));
    }

    @Test
void removesPartialFileWhenSha1DoesNotMatch()
        throws Exception {

    byte[] firmware =
            "corrupted-firmware".getBytes(StandardCharsets.UTF_8);

    httpServer = HttpServer.create(
            new InetSocketAddress("127.0.0.1", 0),
            0);

    httpServer.createContext("/artifact.bin", exchange -> {
        exchange.sendResponseHeaders(200, firmware.length);

        try (var responseBody = exchange.getResponseBody()) {
            responseBody.write(firmware);
        }
    });

    httpServer.start();

    String downloadUrl = "http://127.0.0.1:"
            + httpServer.getAddress().getPort()
            + "/artifact.bin";

    var artifact = new HawkbitDmfDownloadCommand.Artifact(
            "firmware.bin",
            Map.of("HTTP", downloadUrl),
            new HawkbitDmfDownloadCommand.Hashes(
                    "unused",
                    "0000000000000000000000000000000000000000"),
            firmware.length);

    var service = new HawkbitArtifactStagingService(
            temporaryDirectory.toString(),
            1024);

    IOException error = assertThrows(
            IOException.class,
            () -> service.stage(
                    12,
                    23,
                    "testToken123",
                    artifact));

    assertEquals(
            "artifact SHA-1 does not match metadata",
            error.getMessage());
    assertFalse(Files.exists(
            temporaryDirectory.resolve("12/23.bin")));
    assertFalse(Files.exists(
            temporaryDirectory.resolve("12/23.bin.part")));
}

    private static String sha1(byte[] content)
            throws Exception {

        MessageDigest digest =
                MessageDigest.getInstance("SHA-1");

        return HexFormat.of().formatHex(
                digest.digest(content));
    }
}
