package ota.platform.server.security;

import java.util.UUID;

import org.springframework.stereotype.Component;

@Component
public class DatabasePskSecretProvider
        implements PskSecretProvider {

    private static final String PREFIX = "db:";

    private final DeviceCredentialRepository credentialRepository;
    private final PskEncryptionService encryptionService;

    public DatabasePskSecretProvider(
            DeviceCredentialRepository credentialRepository,
            PskEncryptionService encryptionService) {

        this.credentialRepository = credentialRepository;
        this.encryptionService = encryptionService;
    }

    @Override
    public byte[] load(String secretReference) {

        if (secretReference == null ||
                !secretReference.startsWith(PREFIX)) {

            throw new IllegalStateException(
                    "PSK secret reference must use db: prefix");
        }

        UUID credentialId;
        try {
            credentialId = UUID.fromString(
                    secretReference.substring(PREFIX.length()));
        } catch (IllegalArgumentException exception) {
            throw new IllegalStateException(
                    "PSK database reference is invalid",
                    exception);
        }

        byte[] encryptedSecret = credentialRepository
                .findEncryptedSecretById(credentialId)
                .orElseThrow(() -> new IllegalStateException(
                        "encrypted PSK was not found"));

        return encryptionService.decrypt(
                credentialId,
                encryptedSecret);
    }
}
