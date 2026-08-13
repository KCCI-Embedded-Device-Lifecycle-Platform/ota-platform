package ota.platform.server.listener;
import ota.platform.server.hawkbit.HawkbitDmfPublisher;

import java.util.Collection;

import org.eclipse.leshan.core.observation.Observation;
import org.eclipse.leshan.core.request.ObserveRequest;
import org.eclipse.leshan.server.LeshanServer;
import org.eclipse.leshan.server.registration.Registration;
import org.eclipse.leshan.server.registration.RegistrationListener;
import org.eclipse.leshan.server.registration.RegistrationUpdate;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

@Component
public class DeviceRegistrationListener implements RegistrationListener {

    private LeshanServer leshanServer;
    private final HawkbitDmfPublisher hawkbitDmfPublisher;
    private final FirmwareObservationListener firmwareObservationListener;

    private static final Logger logger =
            LoggerFactory.getLogger(DeviceRegistrationListener.class);

 
    public DeviceRegistrationListener(
            HawkbitDmfPublisher hawkbitDmfPublisher,
            FirmwareObservationListener firmwareObservationListener) {
        this.hawkbitDmfPublisher = hawkbitDmfPublisher;
        this.firmwareObservationListener = firmwareObservationListener;
    }

    public void setLeshanServer(LeshanServer leshanServer) {
        this.leshanServer = leshanServer;
    }


    private void observeFirmwareResource(
        Registration registration,
        int resourceId,
        String resourceName) {

    leshanServer.send(
            registration,
            new ObserveRequest(5, 0, resourceId),
            response -> {
                if (response.isSuccess()) {
                    logger.info(
                            "{} observation established: endpoint={}",
                            resourceName,
                            registration.getEndpoint());
                    firmwareObservationListener.handleFirmwareResourceResponse(
                            resourceId,
                            registration,
                            response);
                } else {
                    logger.warn(
                            "{} observation rejected: endpoint={}, response={}",
                            resourceName,
                            registration.getEndpoint(),
                            response);
                }
            },
            error -> logger.warn(
                    "{} observation failed: endpoint={}",
                    resourceName,
                    registration.getEndpoint(),
                    error));
   }

    @Override
    public void registered(
            Registration registration,
            Registration previousRegistration,
            Collection<Observation> previousObservations) {

        logger.info(
                "LwM2M client registered: endpoint={}, address={}",
                registration.getEndpoint(),
                registration.getSocketAddress());

        hawkbitDmfPublisher.publishThingCreated(
                registration.getEndpoint());

        observeFirmwareResource(registration, 3, "Firmware State");
        observeFirmwareResource(registration, 5, "Firmware Update Result");
    }

    @Override
    public void updated(
            RegistrationUpdate update,
            Registration updatedRegistration,
            Registration previousRegistration) {

        logger.info(
                "LwM2M client registration updated: endpoint={}",
                updatedRegistration.getEndpoint());
    }

    @Override
    public void unregistered(
            Registration registration,
            Collection<Observation> observations,
            boolean expired,
            Registration newRegistration) {

        logger.info(
                "LwM2M client unregistered: endpoint={}, expired={}",
                registration.getEndpoint(),
                expired);
    }
}
