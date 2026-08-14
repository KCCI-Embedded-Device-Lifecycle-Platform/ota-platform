package ota.platform.server.security;

public record CreatePskCredentialRequest(
        String identity,
        String keyHex) {
}
