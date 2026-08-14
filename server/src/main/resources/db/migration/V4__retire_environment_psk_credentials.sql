UPDATE device_credentials
SET status = 'REVOKED',
    revoked_at = COALESCE(revoked_at, CURRENT_TIMESTAMP),
    updated_at = CURRENT_TIMESTAMP
WHERE status = 'ACTIVE'
  AND (
      encrypted_secret IS NULL
      OR secret_reference <> 'db:' || id::text
  );

ALTER TABLE device_credentials
    ADD CONSTRAINT ck_device_credentials_active_secret
    CHECK (
        status <> 'ACTIVE'
        OR (
            encrypted_secret IS NOT NULL
            AND secret_reference = 'db:' || id::text
        )
    );