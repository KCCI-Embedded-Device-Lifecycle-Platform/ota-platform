package ota.platform.server.config;

import ota.platform.server.listener.GatewayRegistrationListener;
import org.eclipse.leshan.server.LeshanServer;
import org.eclipse.leshan.server.LeshanServerBuilder;
import org.eclipse.leshan.transport.californium.server.endpoint.CaliforniumServerEndpointsProvider;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class LeshanServerConfiguration {
    @Bean(initMethod = "start", destroyMethod = "destroy")
    public LeshanServer leshanServer(GatewayRegistrationListener registrationListener) {
        //default:5683
        CaliforniumServerEndpointsProvider endpointsProvider =
                new CaliforniumServerEndpointsProvider.Builder().build();
        
        LeshanServer server = new LeshanServerBuilder()
                .setEndpointsProviders(endpointsProvider)
                .build();

        server.getRegistrationService().addListener(registrationListener);
        
        return server;
    }
}
