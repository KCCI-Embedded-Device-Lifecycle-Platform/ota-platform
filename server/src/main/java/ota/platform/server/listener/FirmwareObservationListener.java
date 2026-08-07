package ota.platform.server.listener;

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

@Component
public class FirmwareObservationListener implements ObservationListener {

    private static final Logger logger =
            LoggerFactory.getLogger(FirmwareObservationListener.class);

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
}