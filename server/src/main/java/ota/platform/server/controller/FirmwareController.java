package ota.platform.server.controller;

import java.util.List;
import java.util.Map;
import java.util.ArrayList;


import ota.platform.server.firmware.FirmwareCapabilities;
import ota.platform.server.firmware.FirmwareStatus;
import ota.platform.server.firmware.FirmwareDownloadRequest;
import ota.platform.server.firmware.FirmwareCommandResult;
import ota.platform.server.firmware.FirmwareUpdateDeviceService;

import org.eclipse.leshan.core.node.LwM2mResourceInstance;
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
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;

@RestController
@RequestMapping("/api/devices")
public class FirmwareController {
        private final LeshanServer leshanServer;
        private final FirmwareUpdateDeviceService firmwareUpdateDeviceService;

        public FirmwareController(
                LeshanServer leshanServer,
                FirmwareUpdateDeviceService firmwareUpdateDeviceService) {
                this.leshanServer = leshanServer;
                this.firmwareUpdateDeviceService =
                        firmwareUpdateDeviceService;
        }

        @GetMapping("/{endpoint}/firmware/capabilities")
        public ResponseEntity<?> readFirmwareCapabilities(
                        @PathVariable String endpoint) throws InterruptedException {

                Registration registration = leshanServer.getRegistrationService()
                                .getByEndpoint(endpoint);

                if (registration == null) {
                        return ResponseEntity.notFound().build();
                }

                ReadResponse protocolResponse = leshanServer.send(
                                registration,
                                new ReadRequest(5, 0, 8));

                if (!protocolResponse.isSuccess()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "path", "/5/0/8",
                                                        "error", protocolResponse.toString()));
                }

                if (!(protocolResponse.getContent() instanceof LwM2mResource protocolResource) ||
                                !protocolResource.isMultiInstances() ||
                                protocolResource.getInstances().isEmpty()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "error",
                                                        "Protocol Support is not a non-empty Multiple Resource"));
                }

                List<Integer> protocolSupport = new ArrayList<>();

                for (LwM2mResourceInstance instance : protocolResource.getInstances().values()) {

                        Object value = instance.getValue();

                        if (!(value instanceof Number protocol)) {
                                return ResponseEntity
                                                .status(HttpStatus.BAD_GATEWAY)
                                                .body(Map.of(
                                                                "endpoint", endpoint,
                                                                "error",
                                                                "Protocol Support contains a non-numeric value"));
                        }

                        protocolSupport.add(protocol.intValue());
                }

                ReadResponse deliveryResponse = leshanServer.send(
                                registration,
                                new ReadRequest(5, 0, 9));

                if (!deliveryResponse.isSuccess()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "path", "/5/0/9",
                                                        "error", deliveryResponse.toString()));
                }

                if (!(deliveryResponse.getContent() instanceof LwM2mResource resource) ||
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
                                                protocolSupport,
                                                deliveryMethod.intValue()));

        }

        @GetMapping("/{endpoint}/firmware/status")
        public ResponseEntity<?> readFirmwareStatus(
                        @PathVariable String endpoint) throws InterruptedException {

                Registration registration = leshanServer.getRegistrationService()
                                .getByEndpoint(endpoint);

                if (registration == null) {
                        return ResponseEntity.notFound().build();
                }

                ReadResponse stateResponse = leshanServer.send(
                                registration,
                                new ReadRequest(5, 0, 3));

                if (!stateResponse.isSuccess()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "path", "/5/0/3",
                                                        "error", stateResponse.toString()));
                }

                if (!(stateResponse.getContent() instanceof LwM2mResource stateResource) ||
                                stateResource.isMultiInstances()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "error", "Firmware State is not a single Resource"));
                }

                Object rawState = stateResource.getValue();

                if (!(rawState instanceof Number state)) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "error", "Firmware State is not numeric"));
                }

                ReadResponse updateResultResponse = leshanServer.send(
                                registration,
                                new ReadRequest(5, 0, 5));

                if (!updateResultResponse.isSuccess()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "path", "/5/0/5",
                                                        "error", updateResultResponse.toString()));
                }

                if (!(updateResultResponse.getContent() instanceof LwM2mResource updateResultResource) ||
                                updateResultResource.isMultiInstances()) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "error", "Update Result is not a single Resource"));
                }

                Object rawUpdateResult = updateResultResource.getValue();

                if (!(rawUpdateResult instanceof Number updateResult)) {
                        return ResponseEntity
                                        .status(HttpStatus.BAD_GATEWAY)
                                        .body(Map.of(
                                                        "endpoint", endpoint,
                                                        "error", "Update Result is not numeric"));
                }

                return ResponseEntity.ok(
                                new FirmwareStatus(
                                                endpoint,
                                                state.intValue(),
                                                updateResult.intValue()));

        }

        @PostMapping("/{endpoint}/firmware/download")
        public ResponseEntity<?> startFirmwareDownload(
                @PathVariable String endpoint,
                @RequestBody FirmwareDownloadRequest request)
                throws InterruptedException {

        if (request.packageUri() == null ||
                request.packageUri().isBlank()) {

                return ResponseEntity
                        .badRequest()
                        .body(Map.of("error", "packageUri is required"));
        }

        FirmwareCommandResult result =
                firmwareUpdateDeviceService.requestDownload(
                        endpoint,
                        request.packageUri());

        if (result.status()
                == FirmwareCommandResult.Status.DEVICE_OFFLINE) {
                return ResponseEntity.notFound().build();
        }

        if (!result.accepted()) {
                return ResponseEntity
                        .status(HttpStatus.BAD_GATEWAY)
                        .body(Map.of(
                                "endpoint", endpoint,
                                "path", "/5/0/1",
                                "error", result.detail()));
        }

        return ResponseEntity
                .accepted()
                .body(Map.of(
                        "endpoint", endpoint,
                        "packageUri", request.packageUri(),
                        "status", "accepted"));
        }

        @PostMapping("/{endpoint}/firmware/update")
        public ResponseEntity<?> executeFirmwareUpdate(
                @PathVariable String endpoint)
                throws InterruptedException {

        FirmwareCommandResult result =
                firmwareUpdateDeviceService.requestInstall(endpoint);

        if (result.status()
                == FirmwareCommandResult.Status.DEVICE_OFFLINE) {
                return ResponseEntity.notFound().build();
        }

        if (!result.accepted()) {
                return ResponseEntity
                        .status(HttpStatus.BAD_GATEWAY)
                        .body(Map.of(
                                "endpoint", endpoint,
                                "path", "/5/0/2",
                                "error", result.detail()));
        }

        return ResponseEntity
                .accepted()
                .body(Map.of(
                        "endpoint", endpoint,
                        "status", "accepted"));
        }

}
