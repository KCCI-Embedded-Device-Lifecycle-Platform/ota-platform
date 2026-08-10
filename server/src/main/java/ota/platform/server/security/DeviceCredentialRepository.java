package ota.platform.server.security;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.List;
import java.util.UUID;
import java.util.Optional;

import org.springframework.transaction.annotation.Transactional;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

@Repository
public class DeviceCredentialRepository {

    private final JdbcClient jdbcClient;

    public DeviceCredentialRepository(
            JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public List<ActiveDeviceCredential> findAllActive() {

        return jdbcClient.sql("""
                SELECT
                    credential.id AS credential_id,
                    credential.device_id,
                    device.endpoint,
                    credential.identity,
                    credential.secret_reference
                FROM device_credentials credential
                JOIN devices device
                  ON device.id = credential.device_id
                WHERE credential.status = 'ACTIVE'
                  AND credential.credential_type = 'PSK'
                  AND device.enabled = TRUE
                ORDER BY device.endpoint
                """)
                .query(this::mapCredential)
                .list();
    }

    private ActiveDeviceCredential mapCredential(
            ResultSet resultSet,
            int rowNumber) throws SQLException {

        return new ActiveDeviceCredential(
                resultSet.getObject(
                        "credential_id",
                        UUID.class),
                resultSet.getObject(
                        "device_id",
                        UUID.class),
                resultSet.getString("endpoint"),
                resultSet.getString("identity"),
                resultSet.getString(
                        "secret_reference"));
    }

    public ActiveDeviceCredential createPsk(
            UUID deviceId,
            String endpoint,
            String identity,
            String secretReference) {

        UUID credentialId = UUID.randomUUID();

        int updatedRows = jdbcClient.sql("""
                INSERT INTO device_credentials (
                    id,
                    device_id,
                    credential_type,
                    identity,
                    secret_reference
                )
                VALUES (
                    :credentialId,
                    :deviceId,
                    'PSK',
                    :identity,
                    :secretReference
                )
                """)
                .param("credentialId", credentialId)
                .param("deviceId", deviceId)
                .param("identity", identity)
                .param(
                        "secretReference",
                        secretReference)
                .update();

        if (updatedRows != 1) {
            throw new IllegalStateException(
                    "Failed to create device credential");
        }

        return new ActiveDeviceCredential(
                credentialId,
                deviceId,
                endpoint,
                identity,
                secretReference);
    }

    public Optional<ActiveDeviceCredential> findActivePskByEndpoint(String endpoint) {

        return jdbcClient.sql("""
                SELECT
                    credential.id AS credential_id,
                    credential.device_id,
                    device.endpoint,
                    credential.identity,
                    credential.secret_reference
                FROM device_credentials credential
                JOIN devices device
                ON device.id = credential.device_id
                WHERE device.endpoint = :endpoint
                AND credential.status = 'ACTIVE'
                AND credential.credential_type = 'PSK'
                """)
                .param("endpoint", endpoint)
                .query(this::mapCredential)
                .optional();
    }

    @Transactional
    public Optional<ActiveDeviceCredential>
            rotateActivePsk(
                    UUID deviceId,
                    String endpoint,
                    String identity,
                    String secretReference) {

        int rotatedRows = jdbcClient.sql("""
                UPDATE device_credentials
                SET status = 'ROTATED',
                    updated_at = CURRENT_TIMESTAMP
                WHERE device_id = :deviceId
                AND credential_type = 'PSK'
                AND status = 'ACTIVE'
                """)
                .param("deviceId", deviceId)
                .update();

        if (rotatedRows == 0) {
            return Optional.empty();
        }

        if (rotatedRows != 1) {
            throw new IllegalStateException(
                    "Multiple active PSK credentials found");
        }

        UUID credentialId = UUID.randomUUID();

        int insertedRows = jdbcClient.sql("""
                INSERT INTO device_credentials (
                    id,
                    device_id,
                    credential_type,
                    identity,
                    secret_reference
                )
                VALUES (
                    :credentialId,
                    :deviceId,
                    'PSK',
                    :identity,
                    :secretReference
                )
                """)
                .param("credentialId", credentialId)
                .param("deviceId", deviceId)
                .param("identity", identity)
                .param("secretReference", secretReference)
                .update();

        if (insertedRows != 1) {
            throw new IllegalStateException(
                    "Failed to create rotated credential");
        }

        return Optional.of(
                new ActiveDeviceCredential(
                        credentialId,
                        deviceId,
                        endpoint,
                        identity,
                        secretReference));
    }

    public boolean revokeActivePskByEndpoint(String endpoint) {

        int updatedRows = jdbcClient.sql("""
                UPDATE device_credentials credential
                SET status = 'REVOKED',
                    revoked_at = CURRENT_TIMESTAMP,
                    updated_at = CURRENT_TIMESTAMP
                FROM devices device
                WHERE credential.device_id = device.id
                AND device.endpoint = :endpoint
                AND credential.status = 'ACTIVE'
                AND credential.credential_type = 'PSK'
                """)
                .param("endpoint", endpoint)
                .update();

        return updatedRows == 1;
    }

}