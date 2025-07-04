CREATE TABLE api_keys
(
    id         SERIAL PRIMARY KEY,
    user_id    INTEGER REFERENCES users (id) ON DELETE CASCADE,
    type       VARCHAR(50)  NOT NULL, -- 'S3', etc.
    name       VARCHAR(100) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE s3_api_keys
(
    api_key_id        INTEGER PRIMARY KEY REFERENCES api_keys (id) ON DELETE CASCADE,
    provider          VARCHAR(50) NOT NULL, -- 'AWS', 'Cloudflare R2', etc.
    access_key        TEXT        NOT NULL,
    secret_access_key TEXT        NOT NULL,
    region            VARCHAR(20) NOT NULL,
    endpoint          TEXT DEFAULT NULL
);

CREATE TABLE vault
(
    id          SERIAL PRIMARY KEY,
    type        VARCHAR(12)         NOT NULL CHECK (type IN ('local', 's3')),
    name        VARCHAR(100) UNIQUE NOT NULL,
    description TEXT      DEFAULT NULL,
    owner_id    INTEGER             REFERENCES users (id) ON DELETE SET NULL,
    is_active   BOOLEAN   DEFAULT TRUE,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE local
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    mount_point TEXT NOT NULL
);

CREATE TABLE s3
(
    vault_id   INTEGER REFERENCES vault (id) ON DELETE CASCADE,
    api_key_id INTEGER REFERENCES s3_api_keys (api_key_id) ON DELETE CASCADE,
    bucket     TEXT NOT NULL
);

CREATE TABLE usage
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    total_bytes BIGINT    DEFAULT 0 NOT NULL, -- actual usage
    quota_bytes BIGINT    DEFAULT NULL,       -- soft or hard cap
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE usage_log
(
    id          SERIAL PRIMARY KEY,
    vault_id    INTEGER NOT NULL REFERENCES vault (id),
    measured_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_bytes BIGINT  NOT NULL
);
