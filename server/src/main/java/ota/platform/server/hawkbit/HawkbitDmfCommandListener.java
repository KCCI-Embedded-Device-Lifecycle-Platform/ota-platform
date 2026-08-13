package ota.platform.server.hawkbit;

import ota.platform.server.firmware.FirmwareCommandResult;
import ota.platform.server.firmware.FirmwareUpdateDeviceService;

import java.io.IOException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.amqp.core.Message;
import org.springframework.amqp.core.MessageProperties;
import org.springframework.amqp.rabbit.annotation.RabbitListener;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.stereotype.Component;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

@Component
@ConditionalOnProperty(name = "ota.hawkbit.dmf.enabled", havingValue = "true")
public class HawkbitDmfCommandListener {

    private final ObjectMapper objectMapper = new ObjectMapper();
    private final HawkbitArtifactStagingService stagingService;
    private final HawkbitCoapArtifactServer coapArtifactServer;
    private final FirmwareUpdateDeviceService firmwareUpdateDeviceService;
    private final HawkbitFirmwareActionTracker actionTracker;
    private final HawkbitDmfPublisher dmfPublisher;

    private static final Logger log = LoggerFactory.getLogger(HawkbitDmfCommandListener.class);

    public HawkbitDmfCommandListener(
            HawkbitArtifactStagingService stagingService,
            HawkbitCoapArtifactServer coapArtifactServer,
            FirmwareUpdateDeviceService firmwareUpdateDeviceService,
            HawkbitFirmwareActionTracker actionTracker,
            HawkbitDmfPublisher dmfPublisher) {

        this.stagingService = stagingService;
        this.coapArtifactServer = coapArtifactServer;
        this.firmwareUpdateDeviceService = firmwareUpdateDeviceService;
        this.actionTracker = actionTracker;
        this.dmfPublisher = dmfPublisher;
    }

    @RabbitListener(queues = "${ota.hawkbit.dmf.reply-queue}")
    public void receive(Message message) {
        MessageProperties properties = message.getMessageProperties();

        String type = headerAsString(properties, "type");
        String topic = headerAsString(properties, "topic");
        String endpoint = headerAsString(properties, "thingId");
        String tenant = headerAsString(properties, "tenant");

        log.info(
                "hawkBit DMF command received: type={}, topic={}, "
                        + "thingId={}, tenant={}, bodyBytes={}",
                type,
                topic,
                endpoint,
                tenant,
                message.getBody().length);

        if ("CANCEL_DOWNLOAD".equals(topic)) {
            handleCancel(endpoint, message.getBody());
            return;
        }

        if (!"DOWNLOAD".equals(topic)
                && !"DOWNLOAD_AND_INSTALL".equals(topic)) {
            return;
        }

        long receivedActionId = -1;
        long receivedModuleId = -1;
        long trackedActionId = -1;

        try {
            HawkbitDmfDownloadCommand command = objectMapper.readValue(
                    message.getBody(),
                    HawkbitDmfDownloadCommand.class);

            receivedActionId = command.actionId();

            if (command.softwareModules() == null
                    || command.softwareModules().isEmpty()) {
                log.warn(
                        "hawkBit command has no software module: actionId={}",
                        command.actionId());
                return;
            }

            if (actionTracker.isTracking(
                    endpoint,
                    command.actionId())) {

                log.info(
                        "Duplicate active hawkBit command ignored: "
                                + "endpoint={}, actionId={}",
                        endpoint,
                        command.actionId());
                return;
            }

            HawkbitDmfDownloadCommand.SoftwareModule module = command.softwareModules().getFirst();

            if (module.artifacts() == null
                    || module.artifacts().isEmpty()) {
                log.warn(
                        "hawkBit software module has no artifact: moduleId={}",
                        module.moduleId());
                return;
            }

            receivedModuleId = module.moduleId();

            HawkbitDmfDownloadCommand.Artifact artifact = module.artifacts().getFirst();

            log.info(
                    "hawkBit artifact parsed: actionId={}, moduleId={}, "
                            + "version={}, filename={}, size={}, protocols={}",
                    command.actionId(),
                    module.moduleId(),
                    module.moduleVersion(),
                    artifact.filename(),
                    artifact.size(),
                    artifact.urls() == null
                            ? "[]"
                            : artifact.urls().keySet());

            StagedHawkbitArtifact staged = stagingService.stage(
                    command.actionId(),
                    module.moduleId(),
                    command.targetSecurityToken(),
                    artifact);

            log.info(
                    "hawkBit artifact staged: actionId={}, moduleId={}, "
                            + "path={}, size={}",
                    staged.actionId(),
                    staged.softwareModuleId(),
                    staged.path(),
                    staged.size());

            var coapUri = coapArtifactServer.publish(staged);

            log.info(
                    "hawkBit artifact ready for device download: "
                            + "endpoint={}, actionId={}, uri={}",
                    endpoint,
                    command.actionId(),
                    coapUri);

            HawkbitFirmwareAction firmwareAction = new HawkbitFirmwareAction(
                    endpoint,
                    command.actionId(),
                    module.moduleId(),
                    "DOWNLOAD_AND_INSTALL".equals(topic));

            actionTracker.track(firmwareAction);
            trackedActionId = command.actionId();

            FirmwareCommandResult downloadResult = firmwareUpdateDeviceService.requestDownload(
                    endpoint,
                    coapUri.toString());

            if (!downloadResult.accepted()) {
                dmfPublisher.publishActionStatus(
                        command.actionId(),
                        module.moduleId(),
                        HawkbitDmfActionStatus.Status.ERROR,
                        "LwM2M Package URI Write rejected: "
                                + downloadResult.detail());

                actionTracker.remove(endpoint, command.actionId());

                log.warn(
                        "LwM2M firmware download request rejected: "
                                + "endpoint={}, actionId={}, status={}, detail={}",
                        endpoint,
                        command.actionId(),
                        downloadResult.status(),
                        downloadResult.detail());
                
                return;
            }

            log.info(
                    "LwM2M firmware download request accepted: "
                            + "endpoint={}, actionId={}, packageUri={}",
                    endpoint,
                    command.actionId(),
                    coapUri);

        } catch (JsonProcessingException error) {
            log.warn(
                    "Invalid hawkBit DMF download payload: endpoint={}, error={}",
                    endpoint,
                    error.getMessage());
        } catch (IOException error) {
            if (receivedActionId > 0 && receivedModuleId > 0) {
                dmfPublisher.publishActionStatus(
                        receivedActionId,
                        receivedModuleId,
                        HawkbitDmfActionStatus.Status.ERROR,
                        "Artifact preparation failed: "
                                + error.getMessage());
            }
            
            log.warn(
                    "hawkBit artifact preparation failed: "
                            + "endpoint={}, errorType={}, error={}",
                    endpoint,
                    error.getClass().getSimpleName(),
                    error.getMessage());
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();

            if (trackedActionId > 0) {
                actionTracker.remove(endpoint, trackedActionId);
            }

            log.warn(
                    "hawkBit DMF command processing interrupted: endpoint={}",
                    endpoint);
        }
    }

    private void handleCancel(
            String endpoint,
            byte[] body) {

        try {
            HawkbitDmfCancelCommand command = objectMapper.readValue(
                    body,
                    HawkbitDmfCancelCommand.class);

            HawkbitFirmwareAction action = actionTracker.find(endpoint).orElse(null);

            if (action == null
                    || action.actionId() != command.actionId()) {

                log.warn(
                        "hawkBit cancel has no matching active action: "
                                + "endpoint={}, actionId={}",
                        endpoint,
                        command.actionId());
                return;
            }

            FirmwareCommandResult cancelResult = firmwareUpdateDeviceService.requestCancel(endpoint);

            if (cancelResult.accepted()) {
                log.info(
                        "LwM2M firmware cancel request accepted: "
                                + "endpoint={}, actionId={}",
                        endpoint,
                        command.actionId());
                return;
            }

            dmfPublisher.publishActionStatus(
                    action.actionId(),
                    action.softwareModuleId(),
                    HawkbitDmfActionStatus.Status.CANCEL_REJECTED,
                    "LwM2M Cancel Execute rejected: "
                            + cancelResult.detail());

            log.warn(
                    "LwM2M firmware cancel request rejected: "
                            + "endpoint={}, actionId={}, status={}",
                    endpoint,
                    command.actionId(),
                    cancelResult.status());

        } catch (JsonProcessingException error) {
            log.warn(
                    "Invalid hawkBit DMF cancel payload: "
                            + "endpoint={}, error={}",
                    endpoint,
                    error.getMessage());

        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();

            log.warn(
                    "LwM2M firmware cancel request interrupted: "
                            + "endpoint={}",
                    endpoint);
        } catch (IOException error) {
            log.warn(error.getMessage());
        }
    }

    private String headerAsString(
            MessageProperties properties,
            String name) {

        Object value = properties.getHeader(name);
        return value == null ? "" : value.toString();
    }
}
