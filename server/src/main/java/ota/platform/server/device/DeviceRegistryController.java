package ota.platform.server.device;

import java.util.Map;
import java.util.List;

import org.springframework.dao.DuplicateKeyException;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;

@RestController
@RequestMapping("/api/devices")
public class DeviceRegistryController {

    private final DeviceRepository deviceRepository;

    public DeviceRegistryController(
            DeviceRepository deviceRepository) {
        this.deviceRepository = deviceRepository;
    }

    @PostMapping
    public ResponseEntity<?> createDevice(
            @RequestBody CreateDeviceRequest request) {

        String endpoint = request.endpoint();

        if (endpoint == null ||
            endpoint.isBlank() ||
            !endpoint.equals(endpoint.trim()) ||
            endpoint.length() > 255) {

            return ResponseEntity.badRequest().body(
                    Map.of(
                            "error",
                            "endpoint is invalid"));
        }

        String displayName = request.displayName();

        if (displayName != null &&
            displayName.length() > 255) {

            return ResponseEntity.badRequest().body(
                    Map.of(
                            "error",
                            "displayName is too long"));
        }

        if (displayName != null &&
            displayName.isBlank()) {
            displayName = null;
        }

        Device device =
                deviceRepository.create(
                        endpoint,
                        displayName);

        return ResponseEntity
                .status(HttpStatus.CREATED)
                .body(device);
    }

        @GetMapping("/{endpoint}")
        public ResponseEntity<Device> getDevice(
                @PathVariable String endpoint) {

                return deviceRepository
                        .findByEndpoint(endpoint)
                        .map(ResponseEntity::ok)
                        .orElseGet(() ->
                                ResponseEntity
                                        .notFound()
                                        .build());
        }

        @GetMapping
        public List<Device> getDevices() {
                return deviceRepository.findAll();
        }


        @ExceptionHandler(DuplicateKeyException.class)
        public ResponseEntity<?> handleDuplicateEndpoint() {
                return ResponseEntity
                        .status(HttpStatus.CONFLICT)
                        .body(Map.of(
                                "error",
                                "endpoint already exists"));
        }
}
