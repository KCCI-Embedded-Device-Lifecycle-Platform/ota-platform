package ota.platform.server.lwm2m;

import java.util.LinkedHashMap;
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

@RestController
@RequestMapping("/api/devices")
public class Lwm2mResourceController {

    private final LeshanServer leshanServer;

    public Lwm2mResourceController(
            LeshanServer leshanServer) {

        this.leshanServer = leshanServer;
    }

    @GetMapping(
            "/{endpoint}/lwm2m/"
                    + "{objectId}/{instanceId}/{resourceId}")
    public ResponseEntity<?> readResource(
            @PathVariable String endpoint,
            @PathVariable int objectId,
            @PathVariable int instanceId,
            @PathVariable int resourceId)
            throws InterruptedException {

        if (objectId < 0 || instanceId < 0 || resourceId < 0) {
            return ResponseEntity.badRequest().body(
                    Map.of("error", "LwM2M path is invalid"));
        }

        Registration registration = leshanServer
                .getRegistrationService()
                .getByEndpoint(endpoint);

        if (registration == null) {
            return ResponseEntity.notFound().build();
        }

        ReadResponse response = leshanServer.send(
                registration,
                new ReadRequest(
                        objectId,
                        instanceId,
                        resourceId));

        if (response == null || !response.isSuccess()) {
            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "error",
                            response == null
                                    ? "LwM2M Read timed out"
                                    : response.toString()));
        }

        if (!(response.getContent()
                instanceof LwM2mResource resource)) {

            return ResponseEntity
                    .status(HttpStatus.BAD_GATEWAY)
                    .body(Map.of(
                            "endpoint", endpoint,
                            "error",
                            "response is not a Resource"));
        }

        String path = "/"
                + objectId + "/"
                + instanceId + "/"
                + resourceId;

        Object value;

        if (resource.isMultiInstances()) {
            Map<Integer, Object> instances =
                    new LinkedHashMap<>();

            resource.getInstances().forEach(
                    (id, instance) ->
                            instances.put(
                                    id,
                                    instance.getValue()));

            value = instances;
        } else {
            value = resource.getValue();
        }

        return ResponseEntity.ok(
                new Lwm2mResourceValue(
                        endpoint,
                        path,
                        resource.isMultiInstances(),
                        value));
    }
}