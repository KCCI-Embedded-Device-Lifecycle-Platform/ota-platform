package ota.platform.server.hawkbit;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.UUID;

import org.eclipse.californium.core.coap.CoAP.ResponseCode;
import org.eclipse.californium.core.coap.MediaTypeRegistry;
import org.eclipse.californium.core.CoapExchange;
import org.eclipse.californium.core.CoapResource;
import org.eclipse.californium.core.CoapServer;
import org.eclipse.californium.core.config.CoapConfig;
import org.eclipse.californium.elements.config.Configuration;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

@Component
@ConditionalOnProperty(
        name = "ota.hawkbit.dmf.enabled",
        havingValue = "true")
public class HawkbitCoapArtifactServer {

    private static final Logger log =
            LoggerFactory.getLogger(
                    HawkbitCoapArtifactServer.class);

    private final int port;
    private final String publicHost;
    private final int blockSize;
    private final int maxResourceBodySize;

    private CoapServer server;
    private CoapResource firmwareRoot;

    public HawkbitCoapArtifactServer(
            @Value("${ota.hawkbit.coap-proxy.port}")
            int port,
            @Value("${ota.hawkbit.coap-proxy.public-host}")
            String publicHost,
            @Value("${ota.hawkbit.coap-proxy.block-size}")
            int blockSize,
            @Value("${ota.hawkbit.artifact-max-size-bytes}")
            long maxResourceBodySize) {

        if (port <= 0 || port > 65535) {
            throw new IllegalArgumentException(
                    "CoAP proxy port is invalid");
        }

        if (publicHost == null || publicHost.isBlank()) {
            throw new IllegalArgumentException(
                    "CoAP proxy public host is missing");
        }

        if (blockSize < 16
                || blockSize > 1024
                || Integer.bitCount(blockSize) != 1) {
            throw new IllegalArgumentException(
                    "CoAP block size is invalid");
        }

        if (maxResourceBodySize <= 0
                || maxResourceBodySize > Integer.MAX_VALUE) {
            throw new IllegalArgumentException(
                    "CoAP maximum resource size is invalid");
        }

        this.port = port;
        this.publicHost = publicHost;
        this.blockSize = blockSize;
        this.maxResourceBodySize =
                Math.toIntExact(maxResourceBodySize);
    }

    @PostConstruct
    void start() {
        CoapConfig.register();

        Configuration configuration = new Configuration()
                .set(
                        CoapConfig.PREFERRED_BLOCK_SIZE,
                        blockSize)
                .set(
                        CoapConfig.MAX_RESOURCE_BODY_SIZE,
                        maxResourceBodySize);

        firmwareRoot = new CoapResource("firmware");
        firmwareRoot.getAttributes()
                .setTitle("OTA firmware artifacts");

        server = new CoapServer(configuration, port);
        server.add(firmwareRoot);
        server.start();

        log.info(
                "hawkBit CoAP artifact proxy started: "
                        + "port={}, publicHost={}, blockSize={}",
                port,
                publicHost,
                blockSize);
    }

    public URI publish(StagedHawkbitArtifact artifact)
            throws IOException {

        if (artifact == null) {
            throw new IOException("staged artifact is missing");
        }

        if (firmwareRoot == null) {
            throw new IOException(
                    "CoAP artifact proxy is not started");
        }

        long actualSize = Files.size(artifact.path());

        if (actualSize != artifact.size()) {
            throw new IOException(
                    "staged artifact size has changed");
        }

        String resourceName = artifact.actionId()
                + "-"
                + artifact.softwareModuleId()
                + "-"
                + UUID.randomUUID();

        ArtifactResource resource = new ArtifactResource(
                resourceName,
                artifact.path(),
                artifact.size());

        firmwareRoot.add(resource);

        URI uri;

        try {
            uri = new URI(
                    "coap",
                    null,
                    publicHost,
                    port,
                    "/firmware/" + resourceName,
                    null,
                    null);
        } catch (URISyntaxException error) {
            resource.delete();

            throw new IOException(
                    "CoAP artifact URI is invalid",
                    error);
        }

        log.info(
                "staged artifact published over CoAP: "
                        + "actionId={}, moduleId={}, uri={}",
                artifact.actionId(),
                artifact.softwareModuleId(),
                uri);

        return uri;
    }

    @PreDestroy
    void stop() {
        if (server == null) {
            return;
        }

        server.stop();
        server.destroy();

        log.info("hawkBit CoAP artifact proxy stopped");
    }

    private static final class ArtifactResource
            extends CoapResource {

        private final Path artifactPath;
        private final long expectedSize;

        private ArtifactResource(
                String name,
                Path artifactPath,
                long expectedSize) {

            super(name);

            this.artifactPath = artifactPath;
            this.expectedSize = expectedSize;

            setVisible(false);
            getAttributes().addContentType(
                    MediaTypeRegistry.APPLICATION_OCTET_STREAM);
        }

        @Override
        public void handleGET(CoapExchange exchange) {
            try {
                byte[] content =
                        Files.readAllBytes(artifactPath);

                if (content.length != expectedSize) {
                    exchange.respond(
                            ResponseCode.INTERNAL_SERVER_ERROR);
                    return;
                }

                exchange.respond(
                        ResponseCode.CONTENT,
                        content,
                        MediaTypeRegistry.APPLICATION_OCTET_STREAM);

            } catch (IOException error) {
                log.warn(
                        "Failed to read staged artifact: path={}, error={}",
                        artifactPath,
                        error.getMessage());

                exchange.respond(
                        Files.exists(artifactPath)
                                ? ResponseCode.INTERNAL_SERVER_ERROR
                                : ResponseCode.NOT_FOUND);
            }
        }
    }

}
