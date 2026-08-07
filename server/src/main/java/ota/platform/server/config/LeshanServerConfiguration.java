package ota.platform.server.config;

import ota.platform.server.listener.DeviceRegistrationListener;
import ota.platform.server.listener.FirmwareObservationListener;
import org.eclipse.leshan.server.LeshanServer;
import org.eclipse.leshan.server.LeshanServerBuilder;
import org.eclipse.leshan.transport.californium.server.endpoint.CaliforniumServerEndpointsProvider;
import org.eclipse.leshan.servers.security.SecurityStore;
import org.eclipse.leshan.transport.californium.server.endpoint.coap.CoapServerProtocolProvider;
import org.eclipse.leshan.transport.californium.server.endpoint.coaps.CoapsServerProtocolProvider;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.util.List;
import org.eclipse.leshan.core.model.ObjectLoader;
import org.eclipse.leshan.core.model.ObjectModel;
import org.eclipse.leshan.server.model.StaticModelProvider;


@Configuration
public class LeshanServerConfiguration {
    @Bean(initMethod = "start", destroyMethod = "destroy")
    public LeshanServer leshanServer(
        DeviceRegistrationListener registrationListener,
        FirmwareObservationListener observationListener,
        SecurityStore securityStore) throws Exception {

        List<ObjectModel> models = ObjectLoader.loadDefault();

        models.addAll(
            ObjectLoader.loadDdfResources(
                "/models/",
                new String[] {"bms.xml", "firmware-update-v1_2.xml"},
                true));
        
        //default:5683
        CaliforniumServerEndpointsProvider endpointsProvider =
                new CaliforniumServerEndpointsProvider.Builder(
                    new CoapServerProtocolProvider(),
                    new CoapsServerProtocolProvider()).build();
        
        LeshanServer server = new LeshanServerBuilder()
                .setEndpointsProviders(endpointsProvider)
                .setSecurityStore(securityStore)
                .setObjectModelProvider(new StaticModelProvider(models))
                .build();
        
        registrationListener.setLeshanServer(server);
        server.getRegistrationService().addListener(registrationListener);
        server.getObservationService().addListener(observationListener);

        return server;
    }
}
