package ota.platform.server.listener;

import ota.platform.server.hawkbit.HawkbitDmfActionStatus;
import ota.platform.server.hawkbit.HawkbitDmfPublisher;
import ota.platform.server.hawkbit.HawkbitFirmwareActionTracker;
import ota.platform.server.firmware.FirmwareCommandResult;
import ota.platform.server.firmware.FirmwareUpdateDeviceService;

import org.eclipse.leshan.core.node.LwM2mResource;
import org.eclipse.leshan.core.observation.CompositeObservation;
import org.eclipse.leshan.core.observation.Observation;
import org.eclipse.leshan.core.observation.SingleObservation;
import org.eclipse.leshan.core.response.ObserveCompositeResponse;
import org.eclipse.leshan.core.response.ObserveResponse;
import org.eclipse.leshan.server.observation.ObservationListener;
import org.eclipse.leshan.server.registration.Registration;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import org.springframework.stereotype.Component;
import org.springframework.context.annotation.Lazy;

@Component
public class FirmwareObservationListener implements ObservationListener {

    private final HawkbitFirmwareActionTracker actionTracker;
    private final HawkbitDmfPublisher dmfPublisher;
    private final FirmwareUpdateDeviceService firmwareUpdateDeviceService;

    public FirmwareObservationListener(
            HawkbitFirmwareActionTracker actionTracker,
            HawkbitDmfPublisher dmfPublisher,
            @Lazy FirmwareUpdateDeviceService firmwareUpdateDeviceService) {

        this.actionTracker = actionTracker;
        this.dmfPublisher = dmfPublisher;
        this.firmwareUpdateDeviceService = firmwareUpdateDeviceService;
    }

    private static final Logger logger = LoggerFactory.getLogger(FirmwareObservationListener.class);

    @Override
    public void newObservation(
            Observation observation,
            Registration registration) {

        logger.info(
                "Observation started: endpoint={}, observation={}",
                registration.getEndpoint(),
                observation);
    }

    @Override
    public void cancelled(Observation observation) {
        logger.info("Observation cancelled: {}", observation);
    }

    @Override
    public void onResponse(
            SingleObservation observation,
            Registration registration,
            ObserveResponse response) {

        logger.info(
                "Firmware notification: endpoint={}, path={}, content={}",
                registration.getEndpoint(),
                observation.getPath(),
                response.getContent());
        handleFirmwareResourceResponse(
                observation.getPath().getResourceId(),
                registration,
                response);
    }

    public void handleFirmwareResourceResponse(
            Integer resourceId,
            Registration registration,
            ObserveResponse response) {

        if (resourceId == null) {
            return;
        }

        handleFirmwareState(resourceId, registration, response);
        handleFirmwareUpdateResult(resourceId, registration, response);
    }

    @Override
    public void onResponse(
            CompositeObservation observation,
            Registration registration,
            ObserveCompositeResponse response) {
        // 이번 구현에서는 Composite Observe를 사용하지 않는다.
    }

    @Override
    public void onError(
            Observation observation,
            Registration registration,
            Exception error) {

        logger.warn(
                "Observation error: endpoint={}, observation={}",
                registration.getEndpoint(),
                observation,
                error);
    }

    private void handleFirmwareState(
            int resourceId,
            Registration registration,
            ObserveResponse response) {

        if (resourceId != 3) {
            return;
        }

        if (!(response.getContent() instanceof LwM2mResource resource)
                || resource.isMultiInstances()) {

            logger.warn(
                    "Firmware State notification is invalid: endpoint={}",
                    registration.getEndpoint());
            return;
        }

        Object rawState = resource.getValue();

        if (!(rawState instanceof Number stateNumber)) {
            logger.warn(
                    "Firmware State is not numeric: endpoint={}",
                    registration.getEndpoint());
            return;
        }

        int state = stateNumber.intValue();
        String endpoint = registration.getEndpoint();

        actionTracker.find(endpoint).ifPresent(action -> {
            HawkbitDmfActionStatus.Status status = switch (state) {
                case 1 ->
                    HawkbitDmfActionStatus.Status.DOWNLOAD;
                case 2 ->
                    HawkbitDmfActionStatus.Status.DOWNLOADED;
                case 3 ->
                    HawkbitDmfActionStatus.Status.RUNNING;
                default -> null;
            };

            if (status == null
                    || !actionTracker.markFirmwareState(
                            endpoint,
                            action.actionId(),
                            state)) {
                return;
            }

            dmfPublisher.publishActionStatus(
                    action.actionId(),
                    action.softwareModuleId(),
                    status,
                    "LwM2M Firmware State changed to " + state);

            if (state != 2
                    || !action.installAfterDownload()
                    || !actionTracker.markInstallRequested(
                            endpoint,
                            action.actionId())) {
                return;
            }

            try {
                FirmwareCommandResult installResult = firmwareUpdateDeviceService.requestInstall(
                        endpoint);

                if (!installResult.accepted()) {
                    dmfPublisher.publishActionStatus(
                            action.actionId(),
                            action.softwareModuleId(),
                            HawkbitDmfActionStatus.Status.ERROR,
                            "LwM2M Update Execute rejected: "
                                    + installResult.detail());

                    actionTracker.remove(
                            endpoint,
                            action.actionId());

                    logger.warn(
                            "LwM2M firmware install request rejected: "
                                    + "endpoint={}, actionId={}, status={}",
                            endpoint,
                            action.actionId(),
                            installResult.status());
                    return;
                }

                logger.info(
                        "LwM2M firmware install request accepted: "
                                + "endpoint={}, actionId={}",
                        endpoint,
                        action.actionId());

            } catch (InterruptedException error) {
                Thread.currentThread().interrupt();

                actionTracker.remove(
                        endpoint,
                        action.actionId());

                logger.warn(
                        "LwM2M firmware install request interrupted: "
                                + "endpoint={}, actionId={}",
                        endpoint,
                        action.actionId());
            }
        });
    }

    private void handleFirmwareUpdateResult(
            int resourceId,
            Registration registration,
            ObserveResponse response) {

        if (resourceId != 5) {
            return;
        }

        if (!(response.getContent() instanceof LwM2mResource resource)
                || resource.isMultiInstances()) {

            logger.warn(
                    "Firmware Update Result notification is invalid: "
                            + "endpoint={}",
                    registration.getEndpoint());
            return;
        }

        Object rawResult = resource.getValue();

        if (!(rawResult instanceof Number resultNumber)) {
            logger.warn(
                    "Firmware Update Result is not numeric: endpoint={}",
                    registration.getEndpoint());
            return;
        }

        int updateResult = resultNumber.intValue();

        // 0은 다운로드/업데이트 시작 시 설정되는 초기값이다.
        if (updateResult == 0) {
            return;
        }

        String endpoint = registration.getEndpoint();

        actionTracker.find(endpoint).ifPresent(action -> {
            if (!actionTracker.markUpdateResult(
                    endpoint,
                    action.actionId(),
                    updateResult)) {
                return;
            }

            HawkbitDmfActionStatus.Status status = switch (updateResult) {
                case 1 ->
                    HawkbitDmfActionStatus.Status.FINISHED;
                case 10 ->
                    HawkbitDmfActionStatus.Status.CANCELED;
                case 11 ->
                    HawkbitDmfActionStatus.Status.WARNING;
                default ->
                    HawkbitDmfActionStatus.Status.ERROR;
            };

            dmfPublisher.publishActionStatus(
                    action.actionId(),
                    action.softwareModuleId(),
                    status,
                    "LwM2M Firmware Update Result changed to "
                            + updateResult);

            // Deferred(11)는 아직 작업이 끝난 상태가 아니다.
            if (updateResult != 11) {
                actionTracker.remove(
                        endpoint,
                        action.actionId());
            }
        });
    }

}