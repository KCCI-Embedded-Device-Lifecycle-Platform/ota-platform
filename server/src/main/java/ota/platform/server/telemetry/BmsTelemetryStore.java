package ota.platform.server.telemetry;

import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

import org.springframework.stereotype.Component;

@Component
public class BmsTelemetryStore {

    private final Map<String, BmsTelemetry> latestByEndpoint =
            new ConcurrentHashMap<>();
    
    public void save(BmsTelemetry telemetry) {
        latestByEndpoint.put(telemetry.endpoint(), telemetry);
    }

    public Optional<BmsTelemetry> findLatest(String endpoint) {
        return Optional.ofNullable(
                latestByEndpoint.get(endpoint));
    }
}
