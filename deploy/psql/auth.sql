CREATE TABLE users
(
    id               SERIAL PRIMARY KEY,
    linux_uid        INTEGER UNIQUE,                -- Maps a user to a local UID
    name             VARCHAR(50) UNIQUE NOT NULL,
    email            VARCHAR(255) UNIQUE DEFAULT NULL,
    password_hash    VARCHAR(255)       NOT NULL,
    created_at       TIMESTAMP           DEFAULT CURRENT_TIMESTAMP,
    last_login       TIMESTAMP,
    is_active        BOOLEAN             DEFAULT TRUE
);

CREATE TABLE refresh_tokens
(
    jti        UUID PRIMARY KEY,
    user_id    INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    token_hash TEXT    NOT NULL,
    created_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP DEFAULT NOW() + INTERVAL '7 days',
    last_used  TIMESTAMP DEFAULT NOW(),
    ip_address TEXT,
    user_agent TEXT,
    revoked    BOOLEAN   DEFAULT FALSE
);

CREATE TABLE groups
(
    id          SERIAL PRIMARY KEY,
    linux_gid   INTEGER UNIQUE, -- will add not null constraint later
    name        VARCHAR(50) UNIQUE NOT NULL,
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE group_members
(
    group_id  INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    user_id   INTEGER REFERENCES users (id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (group_id, user_id)
);
