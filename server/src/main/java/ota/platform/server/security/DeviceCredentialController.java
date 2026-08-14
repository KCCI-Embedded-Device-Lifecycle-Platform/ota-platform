package ota.platform.server.security;

import java.util.Map;
import java.util.Optional;

import ota.platform.server.device.Device;
import ota.platform.server.device.DeviceRepository;
import org.springframework.dao.DuplicateKeyException;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/devices/{endpoint}/credentials")
public class DeviceCredentialController {

        private final DeviceRepository deviceRepository;
        private final DeviceCredentialRepository credentialRepository;
        private final DeviceCredentialLifecycleService lifecycleService;

        private boolean isInvalidIdentity(String identity) {
                return identity == null ||
                                identity.isBlank() ||
                                !identity.equals(identity.trim()) ||
                                identity.length() > 255;
        }

        public DeviceCredentialController(
                        DeviceRepository deviceRepository,
                        DeviceCredentialRepository credentialRepository,
                        DeviceCredentialLifecycleService lifecycleService) {

                this.deviceRepository = deviceRepository;
                this.credentialRepository = credentialRepository;
                this.lifecycleService = lifecycleService;
        }

        @GetMapping("/psk")
        public ResponseEntity<ActiveDeviceCredential> getActivePsk(
                        @PathVariable String endpoint) {

                return credentialRepository
                                .findActivePskByEndpoint(endpoint)
                                .map(ResponseEntity::ok)
                                .orElseGet(() -> ResponseEntity.notFound().build());
        }

        @PostMapping("/psk")
        public ResponseEntity<?> createPsk(
                        @PathVariable String endpoint,
                        @RequestBody CreatePskCredentialRequest request) {

                String identity = request.identity();
                String keyHex   = request.keyHex();

                if (isInvalidIdentity(identity)) {
                        return ResponseEntity.badRequest().body(
                                Map.of("error", "identity is invalid"));
                }

                Optional<Device> device = deviceRepository.findByEndpoint(
                                endpoint);

                if (device.isEmpty()) {
                        return ResponseEntity
                                        .notFound()
                                        .build();
                }

                ActiveDeviceCredential credential = 
                        lifecycleService.createStoredPsk(
                        device.get().id(),
                        endpoint,
                        identity,
                        keyHex);

                return ResponseEntity
                                .status(HttpStatus.CREATED)
                                .body(credential);
        }

        @PostMapping("/psk/rotate")
        public ResponseEntity<?> rotatePsk(
                @PathVariable String endpoint,
                @RequestBody
                CreatePskCredentialRequest request) {

        String identity = request.identity();
        String keyHex = request.keyHex();

        if (isInvalidIdentity(identity)) {
                return ResponseEntity.badRequest().body(
                        Map.of(
                                "error",
                                "identity is invalid"));
        }


        Optional<Device> device =
                deviceRepository.findByEndpoint(
                        endpoint);

        if (device.isEmpty()) {
                return ResponseEntity.notFound().build();
        }

        Optional<ActiveDeviceCredential> rotated =
                lifecycleService.rotateStoredPsk(
                        device.get().id(),
                        endpoint,
                        identity,
                        keyHex);

        if (rotated.isEmpty()) {
                return ResponseEntity.notFound().build();
        }

        return ResponseEntity
                .status(HttpStatus.CREATED)
                .body(rotated.get());
        }

        @PostMapping("/psk/revoke")
        public ResponseEntity<Void> revokePsk(
                        @PathVariable String endpoint) {

                boolean revoked = lifecycleService.revokeActivePsk(endpoint);

                if (!revoked) {
                        return ResponseEntity.notFound().build();
                }

                return ResponseEntity.noContent().build();
        }

        @ExceptionHandler(DuplicateKeyException.class)
        public ResponseEntity<?> handleDuplicate() {
                return ResponseEntity
                                .status(HttpStatus.CONFLICT)
                                .body(Map.of(
                                                "error",
                                                "active credential already exists"));
        }

        @ExceptionHandler(IllegalArgumentException.class)
        public ResponseEntity<?> handleInvalidArgument(
                IllegalArgumentException exception) {

        return ResponseEntity
                .badRequest()
                .body(Map.of(
                        "error",
                        exception.getMessage()));
        }
}
