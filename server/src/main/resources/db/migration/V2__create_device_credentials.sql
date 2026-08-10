CREATE TABLE device_credentials (
    id UUID PRIMARY KEY,
    device_id UUID NOT NULL,
    credential_type VARCHAR(16) NOT NULL,
    identity VARCHAR(255) NOT NULL,
    secret_reference VARCHAR(512) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    revoked_at TIMESTAMPTZ,

    CONSTRAINT fk_device_credentials_device
        FOREIGN KEY (device_id)
        REFERENCES devices (id)
        ON DELETE RESTRICT,

    CONSTRAINT ck_device_credentials_type
        CHECK (credential_type IN ('PSK')),

    CONSTRAINT ck_device_credentials_status
        CHECK (status IN (
            'ACTIVE',
            'ROTATED',
            'REVOKED'
        )),

    CONSTRAINT ck_device_credentials_identity_not_blank
        CHECK (btrim(identity) <> ''),

    CONSTRAINT ck_device_credentials_secret_reference_not_blank
        CHECK (btrim(secret_reference) <> '')
);

CREATE UNIQUE INDEX uk_device_credentials_active_device
    ON device_credentials (device_id)
    WHERE status = 'ACTIVE';

CREATE UNIQUE INDEX uk_device_credentials_active_identity
    ON device_credentials (identity)
    WHERE status = 'ACTIVE';