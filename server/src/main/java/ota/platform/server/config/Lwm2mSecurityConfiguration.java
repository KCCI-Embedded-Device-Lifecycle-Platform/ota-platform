package ota.platform.server.config;

import java.util.HexFormat;

import org.eclipse.leshan.servers.security.InMemorySecurityStore;
import org.eclipse.leshan.servers.security.SecurityInfo;
import org.eclipse.leshan.servers.security.SecurityStore;
import org.eclipse.leshan.servers.security.NonUniqueSecurityInfoException;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class Lwm2mSecurityConfiguration {

    @Bean
    public SecurityStore lwm2mSecurityStore(
            @Value("${ota.lwm2m.psk.endpoint:}") String endpoint,
            @Value("${ota.lwm2m.psk.identity:}") String identity,
            @Value("${ota.lwm2m.psk.key-hex:}") String keyHex) {

        InMemorySecurityStore securityStore =
                new InMemorySecurityStore();

        if (endpoint.isBlank() &&
                identity.isBlank() &&
                keyHex.isBlank()) {
            return securityStore;
        }

        if (endpoint.isBlank() ||
                identity.isBlank() ||
                keyHex.isBlank()) {
            throw new IllegalStateException(
                    "DTLS-PSK configuration is incomplete");
        }

        byte[] key;

        try {
            key = HexFormat.of().parseHex(keyHex);
        } catch (IllegalArgumentException error) {
            throw new IllegalStateException(
                    "DTLS-PSK key must be hexadecimal",
                    error);
        }

        if (key.length == 0) {
            throw new IllegalStateException(
                    "DTLS-PSK key must not be empty");
        }

        try {
            securityStore.add(
                    SecurityInfo.newPreSharedKeyInfo(
                            endpoint,
                            identity,
                            key));
        } catch (NonUniqueSecurityInfoException error) {
            throw new IllegalStateException(
                    "DTLS-PSK endpoint or identity is duplicated",
                    error);
        }

        return securityStore;
    }
}