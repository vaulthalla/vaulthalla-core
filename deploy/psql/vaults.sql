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
    id         SERIAL PRIMARY KEY,
    type       VARCHAR(50)         NOT NULL,
    name       VARCHAR(150) UNIQUE NOT NULL,
    is_active  BOOLEAN   DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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

CREATE TABLE volume
(
    id          SERIAL PRIMARY KEY,
    vault_id    INTEGER      NOT NULL,
    name        VARCHAR(150) NOT NULL,
    path_prefix VARCHAR(255) DEFAULT '/',
    quota_bytes BIGINT       DEFAULT NULL,
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE volumes
(
    subject_id  INTEGER NOT NULL, -- Can be user or group
    subject_type VARCHAR(10) NOT NULL CHECK (subject_type IN ('user', 'group')),
    volume_id   INTEGER REFERENCES volume (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (subject_id, subject_type, volume_id)
);

CREATE TABLE storage_usage
(
    volume_id   INTEGER PRIMARY KEY REFERENCES volume (id) ON DELETE CASCADE,
    total_bytes BIGINT    DEFAULT 0 NOT NULL, -- actual usage
    quota_bytes BIGINT    DEFAULT NULL,       -- soft or hard cap
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE storage_usage_log
(
    id          SERIAL PRIMARY KEY,
    volume_id   INTEGER NOT NULL REFERENCES volume (id),
    measured_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_bytes BIGINT  NOT NULL
);
