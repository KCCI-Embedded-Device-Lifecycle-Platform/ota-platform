package ota.platform.server.security;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import java.util.Objects;
import java.util.UUID;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;

import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

@Service
public class PskEncryptionService {

    private static final int AES_256_KEY_BYTES = 32;

    private final SecretKey masterKey;
    private static final byte FORMAT_VERSION = 1;
    private static final int GCM_NONCE_BYTES = 12;
    private static final int GCM_TAG_BITS = 128;

    private final SecureRandom secureRandom = new SecureRandom();

    public PskEncryptionService(
            @Value("${ota.credentials.master-key-file}") String masterKeyFile)
            throws IOException {

        String keyHex = Files.readString(Path.of(masterKeyFile)).trim();

        byte[] keyBytes;
        try {
            keyBytes = HexFormat.of().parseHex(keyHex);
        } catch (IllegalArgumentException exception) {
            throw new IllegalStateException(
                    "credential master key must be hexadecimal",
                    exception);
        }

        if (keyBytes.length != AES_256_KEY_BYTES) {
            throw new IllegalStateException(
                    "credential master key must be 32 bytes");
        }

        this.masterKey = new SecretKeySpec(keyBytes, "AES");
        Arrays.fill(keyBytes, (byte) 0);
    }

    public byte[] encrypt(UUID credentialId, byte[] plaintext) {
        Objects.requireNonNull(credentialId, "credentialId");
        Objects.requireNonNull(plaintext, "plaintext");

        if (plaintext.length == 0) {
            throw new IllegalArgumentException("PSK must not be empty");
        }

        byte[] nonce = new byte[GCM_NONCE_BYTES];
        secureRandom.nextBytes(nonce);

        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(
                    Cipher.ENCRYPT_MODE,
                    masterKey,
                    new GCMParameterSpec(GCM_TAG_BITS, nonce));

            cipher.updateAAD(
                    credentialId.toString().getBytes(StandardCharsets.UTF_8));

            byte[] ciphertext = cipher.doFinal(plaintext);

            return ByteBuffer.allocate(
                    1 + GCM_NONCE_BYTES + ciphertext.length)
                    .put(FORMAT_VERSION)
                    .put(nonce)
                    .put(ciphertext)
                    .array();
        } catch (GeneralSecurityException exception) {
            throw new IllegalStateException(
                    "failed to encrypt device PSK",
                    exception);
        }
    }

    public byte[] decrypt(UUID credentialId, byte[] payload) {
        Objects.requireNonNull(credentialId, "credentialId");
        Objects.requireNonNull(payload, "payload");

        int minimumLength = 1 + GCM_NONCE_BYTES + (GCM_TAG_BITS / 8);
        if (payload.length < minimumLength) {
            throw new IllegalArgumentException(
                    "encrypted PSK payload is malformed");
        }

        ByteBuffer buffer = ByteBuffer.wrap(payload);

        byte formatVersion = buffer.get();
        if (formatVersion != FORMAT_VERSION) {
            throw new IllegalArgumentException(
                    "unsupported encrypted PSK format version");
        }

        byte[] nonce = new byte[GCM_NONCE_BYTES];
        buffer.get(nonce);

        byte[] ciphertext = new byte[buffer.remaining()];
        buffer.get(ciphertext);

        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(
                    Cipher.DECRYPT_MODE,
                    masterKey,
                    new GCMParameterSpec(GCM_TAG_BITS, nonce));

            cipher.updateAAD(
                    credentialId.toString().getBytes(StandardCharsets.UTF_8));

            return cipher.doFinal(ciphertext);
        } catch (GeneralSecurityException exception) {
            throw new IllegalStateException(
                    "failed to decrypt device PSK",
                    exception);
        }
    }

}