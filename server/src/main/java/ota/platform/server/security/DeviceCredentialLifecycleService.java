package ota.platform.server.security;

import java.util.UUID;
import java.util.Optional;

import org.eclipse.leshan.servers.security.NonUniqueSecurityInfoException;
import org.eclipse.leshan.servers.security.SecurityInfo;
import org.eclipse.leshan.servers.security.EditableSecurityStore;
import org.springframework.stereotype.Service;

@Service
public class DeviceCredentialLifecycleService {

    private final DeviceCredentialRepository credentialRepository;
    private final EditableSecurityStore securityStore;
    private final PskSecretProvider pskSecretProvider;

    public DeviceCredentialLifecycleService(
            DeviceCredentialRepository credentialRepository,
            EditableSecurityStore securityStore,
            PskSecretProvider pskSecretProvider) {

        this.credentialRepository = credentialRepository;
        this.securityStore = securityStore;
        this.pskSecretProvider = pskSecretProvider;
    }

    public ActiveDeviceCredential createActivePsk(
            UUID deviceId,
            String endpoint,
            String identity,
            String secretReference) {

        /*
        * Resolve the key before changing the DB.
        * A missing environment variable must not create
        * an unusable ACTIVE credential.
        */
        byte[] key = pskSecretProvider.load(secretReference);

        ActiveDeviceCredential credential =
                credentialRepository.createPsk(
                        deviceId,
                        endpoint,
                        identity,
                        secretReference);

        try {
            /*
            * Remove any stale in-memory credential and
            * terminate its DTLS session before adding
            * the new credential.
            */
            securityStore.remove(endpoint, true);

            securityStore.add(
                    SecurityInfo.newPreSharedKeyInfo(
                            endpoint,
                            identity,
                            key));
        }
        catch (NonUniqueSecurityInfoException error) {
            credentialRepository
                    .revokeActivePskByEndpoint(endpoint);

            throw new IllegalStateException(
                    "Unable to activate DTLS-PSK for endpoint "
                            + endpoint,
                    error);
        }

        return credential;
    }

    public Optional<ActiveDeviceCredential>
            rotateActivePsk(
                    UUID deviceId,
                    String endpoint,
                    String identity,
                    String secretReference) {

        /*
        * Validate and load the new key before
        * changing the current credential.
        */
        byte[] key = pskSecretProvider.load(secretReference);

        Optional<ActiveDeviceCredential> rotated =
                credentialRepository.rotateActivePsk(
                        deviceId,
                        endpoint,
                        identity,
                        secretReference);

        if (rotated.isEmpty()) {
            return Optional.empty();
        }

        /*
        * Terminate the session authenticated with
        * the previous credential.
        */
        securityStore.remove(endpoint, true);

        try {
            securityStore.add(
                    SecurityInfo.newPreSharedKeyInfo(
                            endpoint,
                            identity,
                            key));
        }
        catch (NonUniqueSecurityInfoException error) {
            /*
            * Fail closed: do not leave a DB credential
            * ACTIVE when it was not installed in memory.
            */
            credentialRepository
                    .revokeActivePskByEndpoint(endpoint);

            throw new IllegalStateException(
                    "Unable to activate rotated DTLS-PSK for endpoint "
                            + endpoint,
                    error);
        }

        return rotated;
    }

    public boolean revokeActivePsk(String endpoint) {

        boolean revoked = credentialRepository.revokeActivePskByEndpoint(endpoint);

        if (!revoked) {
            return false;
        }

        securityStore.remove(endpoint, true);
        return true;
    }

    
}