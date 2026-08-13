package ota.platform.server.hawkbit;

import java.nio.charset.StandardCharsets;
import java.util.List;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.amqp.AmqpException;
import org.springframework.amqp.core.Message;
import org.springframework.amqp.core.MessageProperties;
import org.springframework.amqp.rabbit.core.RabbitTemplate;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

@Component
public class HawkbitDmfPublisher {

    private final ObjectMapper objectMapper = new ObjectMapper();

    private static final Logger log =
            LoggerFactory.getLogger(HawkbitDmfPublisher.class);

    private final RabbitTemplate rabbitTemplate;
    private final boolean enabled;
    private final String hawkbitExchange;
    private final String replyExchange;
    private final String tenant;

    public HawkbitDmfPublisher(
            RabbitTemplate rabbitTemplate,
            @Value("${ota.hawkbit.dmf.enabled:false}") boolean enabled,
            @Value("${ota.hawkbit.dmf.hawkbit-exchange}") String hawkbitExchange,
            @Value("${ota.hawkbit.dmf.reply-exchange}") String replyExchange,
            @Value("${ota.hawkbit.dmf.tenant}") String tenant) {
        this.rabbitTemplate = rabbitTemplate;
        this.enabled = enabled;
        this.hawkbitExchange = hawkbitExchange;
        this.replyExchange = replyExchange;
        this.tenant = tenant;
    }

    public void publishThingCreated(String endpoint) {
        if (!enabled) {
            return;
        }

        MessageProperties properties = new MessageProperties();
        properties.setContentType(MessageProperties.CONTENT_TYPE_JSON);
        properties.setReplyTo(replyExchange);
        properties.setHeader("type", "THING_CREATED");
        properties.setHeader("thingId", endpoint);
        properties.setHeader("tenant", tenant);
        properties.setHeader("sender", "ota-lwm2m-server");

        Message message = new Message(
                "{}".getBytes(StandardCharsets.UTF_8),
                properties);

        try {
            rabbitTemplate.send(hawkbitExchange, "", message);

            log.info(
                    "hawkBit THING_CREATED published: endpoint={}",
                    endpoint);
        } catch (AmqpException error) {
            log.warn(
                    "Failed to publish hawkBit THING_CREATED: endpoint={}, error={}",
                    endpoint,
                    error.getMessage());
        }
    }

    public void publishActionStatus(
            long actionId,
            long softwareModuleId,
            HawkbitDmfActionStatus.Status status,
            String detail) {

        if (!enabled) {
            return;
        }

        String messageText =
                detail == null || detail.isBlank()
                        ? status.name()
                        : detail;

        HawkbitDmfActionStatus body =
                new HawkbitDmfActionStatus(
                        actionId,
                        softwareModuleId,
                        status,
                        List.of(messageText),
                        System.currentTimeMillis());

        try {
            byte[] payload =
                    objectMapper.writeValueAsBytes(body);

            MessageProperties properties =
                    new MessageProperties();

            properties.setContentType(
                    MessageProperties.CONTENT_TYPE_JSON);
            properties.setHeader("type", "EVENT");
            properties.setHeader(
                    "topic",
                    "UPDATE_ACTION_STATUS");
            properties.setHeader("tenant", tenant);
            properties.setHeader(
                    "sender",
                    "ota-lwm2m-server");

            rabbitTemplate.send(
                    hawkbitExchange,
                    "",
                    new Message(payload, properties));

            log.info(
                    "hawkBit action status published: "
                            + "actionId={}, moduleId={}, status={}",
                    actionId,
                    softwareModuleId,
                    status);

        } catch (JsonProcessingException | AmqpException error) {
            log.warn(
                    "Failed to publish hawkBit action status: "
                            + "actionId={}, status={}, error={}",
                    actionId,
                    status,
                    error.getMessage());
        }
    }

}