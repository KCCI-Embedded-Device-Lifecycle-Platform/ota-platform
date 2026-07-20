package ota.platform.server.controller;
import java.util.Map;

import org.eclipse.leshan.core.node.LwM2mResource;
import org.eclipse.leshan.core.request.ReadRequest;
import org.eclipse.leshan.core.response.ReadResponse;
import org.eclipse.leshan.server.LeshanServer;
import org.eclipse.leshan.server.registration.Registration;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.time.Instant;
import ota.platform.server.telemetry.BmsTelemetry;

import java.util.Optional;
import ota.platform.server.telemetry.BmsTelemetryStore;

@RestController
@RequestMapping("/api/devices")
public class DeviceController {

    private final LeshanServer leshanServer;
    private final BmsTelemetryStore telemetryStore;

    public DeviceController(LeshanServer leshanServer, BmsTelemetryStore telemetryStore) {
        this.leshanServer = leshanServer;
        this.telemetryStore = telemetryStore;
    }

    @GetMapping("/{endpoint}/bms/voltage")
    public ResponseEntity<?> readBmsVoltage(
            @PathVariable String endpoint) throws InterruptedException {
        
        Registration registration =
                leshanServer.getRegistrationService()
                            .getByEndpoint(endpoint);
        
        if (registration == null) {
            return ResponseEntity.notFound().build();
        }

        ReadResponse response = leshanServer.send(
            registration, new ReadRequest(33000, 0, 0));
    
        if(!response.isSuccess()) {
            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "error", response.toString()));
        }

        LwM2mResource resource = (LwM2mResource) response.getContent();
        
        Object rawValue = resource.getValue();

        if(!(rawValue instanceof Number number)) {
            return ResponseEntity
                .status(HttpStatus.BAD_GATEWAY)
                .body(Map.of(
                    "endpoint", endpoint,
                    "error", "BMS voltage is not a numeric value")); 
        }

        BmsTelemetry telemetry = new BmsTelemetry(
            endpoint, number.doubleValue(),
            "V", Instant.now());

        telemetryStore.save(telemetry);

        return ResponseEntity.ok(telemetry);

        /* 
        return ResponseEntity.ok(Map.of(
            "endpoint", endpoint,
            "path", "/33000/0/0",
            "voltage", resource.getValue(),
            "unit", "V"));
        */
    }

    @GetMapping("/{endpoint}/bms/voltage/latest")
    public ResponseEntity<BmsTelemetry> getLatestBmsVoltage(
            @PathVariable String endpoint) {

        Optional<BmsTelemetry> telemetry =
                telemetryStore.findLatest(endpoint);

        if (telemetry.isEmpty()) {
            return ResponseEntity.notFound().build();
        }

        return ResponseEntity.ok(telemetry.get());
    }
    
}
