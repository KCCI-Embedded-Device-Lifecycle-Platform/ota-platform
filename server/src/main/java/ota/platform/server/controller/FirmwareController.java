package ota.platform.server.controller;

import java.util.List;
import java.util.Map;

import ota.platform.server.firmware.FirmwareCapabilities;
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

@RestController
@RequestMapping("/api/devices")
public class FirmwareController {
    private final LeshanServer leshanServer;

    public FirmwareController(LeshanServer leshanServer) {
        this.leshanServer = leshanServer;
    }

    @GetMapping("/{endpoint}/firmware/capabilities")
    public ResponseEntity<?> readFirmwareCapabilities(
        @PathVariable String endpoint) throws InterruptedException {
        
        Registration registration =
                leshanServer.getRegistrationService()
                            .getByEndpoint(endpoint);
        
        if (registration == null) {
            return ResponseEntity.notFound().build();
        }

        ReadResponse response = leshanServer.send(
                registration,
                new ReadRequest(5, 0, 9));
        
        if (!response.isSuccess()) {
            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "path", "/5/0/9",
                            "error", response.toString()));
        }

        if (!(response.getContent() instanceof LwM2mResource resource) ||
                resource.isMultiInstances()) {
            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "error", "Delivery Method is not a single Resource"));
        }

        Object rawValue = resource.getValue();

        if (!(rawValue instanceof Number deliveryMethod)) {
            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "error", "Delivery Method is not numeric"));
        }

        return ResponseEntity.ok(
                new FirmwareCapabilities(
                        endpoint,
                        List.of(),
                        deliveryMethod.intValue()));

    } 
}
