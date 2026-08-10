package ota.platform.server.security;

import java.util.HexFormat;
import java.util.regex.Pattern;

import org.springframework.stereotype.Component;

@Component
public class EnvironmentPskSecretProvider
        implements PskSecretProvider {

    private static final String PREFIX = "env:";

    private static final Pattern ENVIRONMENT_NAME =
            Pattern.compile("[A-Z][A-Z0-9_]*");

    @Override
    public byte[] load(String secretReference) {

        if (secretReference == null ||
            !secretReference.startsWith(PREFIX)) {

            throw new IllegalStateException(
                    "PSK secret reference must use env: prefix");
        }

        String variableName =
                secretReference.substring(PREFIX.length());

        if (!ENVIRONMENT_NAME
                .matcher(variableName)
                .matches()) {

            throw new IllegalStateException(
                    "PSK environment variable name is invalid");
        }

        String keyHex = System.getenv(variableName);

        if (keyHex == null || keyHex.isBlank()) {
            throw new IllegalStateException(
                    "PSK environment variable is missing");
        }

        byte[] key;

        try {
            key = HexFormat.of().parseHex(keyHex);
        } catch (IllegalArgumentException error) {
            throw new IllegalStateException(
                    "PSK environment variable must be hexadecimal",
                    error);
        }

        if (key.length == 0) {
            throw new IllegalStateException(
                    "PSK must not be empty");
        }

        return key;
    }
}