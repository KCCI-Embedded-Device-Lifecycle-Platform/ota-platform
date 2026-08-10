package ota.platform.server.security;

public interface PskSecretProvider {

    byte[] load(String secretReference);
}