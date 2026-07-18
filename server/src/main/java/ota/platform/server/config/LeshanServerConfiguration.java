package ota.platform.server.config;

import ota.platform.server.listener.GatewayRegistrationListener;
import org.eclipse.leshan.server.LeshanServer;
import org.eclipse.leshan.server.LeshanServerBuilder;
import org.eclipse.leshan.transport.californium.server.endpoint.CaliforniumServerEndpointsProvider;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.util.List;
import org.eclipse.leshan.core.model.ObjectLoader;
import org.eclipse.leshan.core.model.ObjectModel;
import org.eclipse.leshan.server.model.StaticModelProvider;


@Configuration
public class LeshanServerConfiguration {
    @Bean(initMethod = "start", destroyMethod = "destroy")
    public LeshanServer leshanServer(GatewayRegistrationListener registrationListener) throws Exception {
        List<ObjectModel> models = ObjectLoader.loadDefault();

        models.addAll(
            ObjectLoader.loadDdfResources(
                "/models/",
                new String[] {"bms.xml"},
                true));
        
        //default:5683
        CaliforniumServerEndpointsProvider endpointsProvider =
                new CaliforniumServerEndpointsProvider.Builder().build();
        
        LeshanServer server = new LeshanServerBuilder()
                .setEndpointsProviders(endpointsProvider)
                .setObjectModelProvider(new StaticModelProvider(models))
                .build();

        server.getRegistrationService().addListener(registrationListener);
        
        return server;
    }
}
