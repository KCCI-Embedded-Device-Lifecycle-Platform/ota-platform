package ota.platform.server.security;

import java.util.UUID;
import java.util.Optional;
import java.util.HexFormat;

import org.eclipse.leshan.servers.security.NonUniqueSecurityInfoException;
import org.eclipse.leshan.servers.security.SecurityInfo;
import org.eclipse.leshan.servers.security.EditableSecurityStore;
import org.springframework.stereotype.Service;

@Service
public class DeviceCredentialLifecycleService {

        private final DeviceCredentialRepository credentialRepository;
        private final EditableSecurityStore securityStore;
        private final PskEncryptionService encryptionService;

        public DeviceCredentialLifecycleService(
                        DeviceCredentialRepository credentialRepository,
                        EditableSecurityStore securityStore,
                        PskEncryptionService encryptionService) {

                this.credentialRepository = credentialRepository;
                this.securityStore = securityStore;
                this.encryptionService = encryptionService;
        }

        public boolean revokeActivePsk(String endpoint) {

                boolean revoked = credentialRepository.revokeActivePskByEndpoint(endpoint);

                if (!revoked) {
                        return false;
                }

                securityStore.remove(endpoint, true);
                return true;
        }

        public ActiveDeviceCredential createStoredPsk(
                        UUID deviceId,
                        String endpoint,
                        String identity,
                        String keyHex) {

                byte[] key = decodePsk(keyHex);

                if (key.length == 0) {
                        throw new IllegalArgumentException(
                                        "PSK must not be empty");
                }

                UUID credentialId = UUID.randomUUID();

                byte[] encryptedSecret = encryptionService.encrypt(
                                credentialId,
                                key);

                ActiveDeviceCredential credential = credentialRepository.createEncryptedPsk(
                                credentialId,
                                deviceId,
                                endpoint,
                                identity,
                                encryptedSecret);

                try {
                        securityStore.remove(endpoint, true);

                        securityStore.add(
                                        SecurityInfo.newPreSharedKeyInfo(
                                                        endpoint,
                                                        identity,
                                                        key));
                } catch (NonUniqueSecurityInfoException exception) {
                        credentialRepository
                                        .revokeActivePskByEndpoint(endpoint);

                        throw new IllegalStateException(
                                        "Unable to activate DTLS-PSK for endpoint "
                                                        + endpoint,
                                        exception);
                }

                return credential;
        }

        public Optional<ActiveDeviceCredential> rotateStoredPsk(
                        UUID deviceId,
                        String endpoint,
                        String identity,
                        String keyHex) {

                byte[] key = decodePsk(keyHex);
                UUID credentialId = UUID.randomUUID();

                byte[] encryptedSecret = encryptionService.encrypt(
                                credentialId,
                                key);

                Optional<ActiveDeviceCredential> rotated = credentialRepository.rotateEncryptedPsk(
                                credentialId,
                                deviceId,
                                endpoint,
                                identity,
                                encryptedSecret);

                if (rotated.isEmpty()) {
                        return Optional.empty();
                }

                securityStore.remove(endpoint, true);

                try {
                        securityStore.add(
                                        SecurityInfo.newPreSharedKeyInfo(
                                                        endpoint,
                                                        identity,
                                                        key));
                } catch (NonUniqueSecurityInfoException exception) {
                        credentialRepository
                                        .revokeActivePskByEndpoint(endpoint);

                        throw new IllegalStateException(
                                        "Unable to activate rotated DTLS-PSK for endpoint "
                                                        + endpoint,
                                        exception);
                }

                return rotated;
        }

        private byte[] decodePsk(String keyHex) {

                if (keyHex == null || keyHex.isBlank()) {
                        throw new IllegalArgumentException(
                                        "PSK must not be empty");
                }

                byte[] key;
                try {
                        key = HexFormat.of().parseHex(keyHex);
                } catch (IllegalArgumentException exception) {
                        throw new IllegalArgumentException(
                                        "PSK must be hexadecimal",
                                        exception);
                }

                if (key.length < 16 || key.length > 64) {
                        throw new IllegalArgumentException(
                                        "PSK must be between 16 and 64 bytes");
                }

                return key;
        }
}