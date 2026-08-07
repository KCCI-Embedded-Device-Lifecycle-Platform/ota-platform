CREATE TABLE devices (
    id UUID PRIMARY KEY,
    endpoint VARCHAR(255) NOT NULL,
    display_name VARCHAR(255),
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT uk_devices_endpoint
        UNIQUE (endpoint),

    CONSTRAINT ck_devices_endpoint_not_blank
        CHECK (btrim(endpoint) <> '')
);