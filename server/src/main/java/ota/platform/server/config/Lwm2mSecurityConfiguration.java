package ota.platform.server.config;
import ota.platform.server.security.PskSecretProvider;
import ota.platform.server.security.ActiveDeviceCredential;
import ota.platform.server.security.DeviceCredentialRepository;


import org.eclipse.leshan.servers.security.InMemorySecurityStore;
import org.eclipse.leshan.servers.security.EditableSecurityStore;
import org.eclipse.leshan.servers.security.SecurityInfo;
import org.eclipse.leshan.servers.security.NonUniqueSecurityInfoException;
import org.springframework.boot.sql.init.dependency.DependsOnDatabaseInitialization;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class Lwm2mSecurityConfiguration {

    @Bean
    @DependsOnDatabaseInitialization
    public EditableSecurityStore lwm2mSecurityStore(
            DeviceCredentialRepository credentialRepository,
            PskSecretProvider pskSecretProvider) {

        InMemorySecurityStore securityStore =
        new InMemorySecurityStore();

        for (ActiveDeviceCredential credential :
                credentialRepository.findAllActive()) {

            byte[] key;

            try {
                key = pskSecretProvider.load(
                        credential.secretReference());
            } catch (IllegalStateException error) {
                throw new IllegalStateException(
                        "Unable to load DTLS-PSK for endpoint " +
                                credential.endpoint(),
                        error);
            }

            try {
                securityStore.add(
                        SecurityInfo.newPreSharedKeyInfo(
                                credential.endpoint(),
                                credential.identity(),
                                key));
            } catch (NonUniqueSecurityInfoException error) {
                throw new IllegalStateException(
                        "DTLS-PSK endpoint or identity is duplicated",
                        error);
            }
        }

        return securityStore;
    }
}