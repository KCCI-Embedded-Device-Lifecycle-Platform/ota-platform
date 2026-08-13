package ota.platform.server.config;

import org.springframework.amqp.core.Binding;
import org.springframework.amqp.core.BindingBuilder;
import org.springframework.amqp.core.FanoutExchange;
import org.springframework.amqp.core.Queue;
import org.springframework.amqp.core.QueueBuilder;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration(proxyBeanMethods = false)
@ConditionalOnProperty(
        name = "ota.hawkbit.dmf.enabled",
        havingValue = "true")
public class HawkbitDmfAmqpConfiguration {

    private final String replyExchangeName;
    private final String replyQueueName;

    public HawkbitDmfAmqpConfiguration(
            @Value("${ota.hawkbit.dmf.reply-exchange}")
            String replyExchangeName,
            @Value("${ota.hawkbit.dmf.reply-queue}")
            String replyQueueName) {
        this.replyExchangeName = replyExchangeName;
        this.replyQueueName = replyQueueName;
    }

    @Bean
    FanoutExchange hawkbitDmfReplyExchange() {
        return new FanoutExchange(replyExchangeName, true, false);
    }

    @Bean
    Queue hawkbitDmfReplyQueue() {
        return QueueBuilder.durable(replyQueueName).build();
    }

    @Bean
    Binding hawkbitDmfReplyBinding(
            @Qualifier("hawkbitDmfReplyQueue") Queue queue,
            @Qualifier("hawkbitDmfReplyExchange") FanoutExchange exchange) {
        return BindingBuilder.bind(queue).to(exchange);
    }
}