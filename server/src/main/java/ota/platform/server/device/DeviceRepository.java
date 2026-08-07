package ota.platform.server.device;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.time.OffsetDateTime;
import java.util.UUID;
import java.util.Optional;
import java.util.List;

import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

@Repository
public class DeviceRepository {

    private final JdbcClient jdbcClient;

    public DeviceRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public Device create(String endpoint,String displayName)
    {
        UUID id = UUID.randomUUID();

        return jdbcClient.sql("""
                INSERT INTO devices (
                    id,
                    endpoint,
                    display_name
                )
                VALUES (
                    :id,
                    :endpoint,
                    :displayName
                )
                RETURNING
                    id,
                    endpoint,
                    display_name,
                    enabled,
                    created_at,
                    updated_at
                """)
                .param("id", id)
                .param("endpoint", endpoint)
                .param("displayName", displayName)
                .query(this::mapDevice)
                .single();
    }

    private Device mapDevice(ResultSet resultSet, int rowNumber) throws SQLException
    {

        return new Device(
                resultSet.getObject("id", UUID.class),
                resultSet.getString("endpoint"),
                resultSet.getString("display_name"),
                resultSet.getBoolean("enabled"),
                resultSet
                        .getObject(
                                "created_at",
                                OffsetDateTime.class)
                        .toInstant(),
                resultSet
                        .getObject(
                                "updated_at",
                                OffsetDateTime.class)
                        .toInstant());
    }

    public Optional<Device> findByEndpoint(String endpoint)
    {
        return jdbcClient.sql("""
                SELECT
                    id,
                    endpoint,
                    display_name,
                    enabled,
                    created_at,
                    updated_at
                FROM devices
                WHERE endpoint = :endpoint
                """)
                .param("endpoint", endpoint)
                .query(this::mapDevice)
                .optional();
    }

    public List<Device> findAll()
    {
        return jdbcClient.sql("""
                SELECT
                    id,
                    endpoint,
                    display_name,
                    enabled,
                    created_at,
                    updated_at
                FROM devices
                ORDER BY created_at, endpoint
                """)
                .query(this::mapDevice)
                .list();
    }

}