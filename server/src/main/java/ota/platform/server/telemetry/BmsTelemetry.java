package ota.platform.server.telemetry;

import java.time.Instant;

public record BmsTelemetry (String endpoint, double voltage, String unit, Instant collectedAt) {

}
