CREATE TABLE permissions
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) UNIQUE NOT NULL,
    description  TEXT,
    category     VARCHAR(12)        NOT NULL CHECK (category IN ('admin', 'vault', 'file', 'directory')),
    bit_position INTEGER            NOT NULL CHECK (bit_position >= 0 AND bit_position < 64),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE role
(
    id                    SERIAL PRIMARY KEY,
    name                  VARCHAR(50) UNIQUE NOT NULL,
    description           TEXT,
    vault_permissions     BIT(16)            NOT NULL,
    file_permissions      BIT(16)            NOT NULL,
    directory_permissions BIT(16)            NOT NULL,
    created_at            TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE roles
(
    id           SERIAL PRIMARY KEY,
    subject_type VARCHAR(10) NOT NULL CHECK (subject_type IN ('user', 'group')),
    subject_id   INTEGER     NOT NULL,
    role_id      INTEGER     NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    assigned_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    inherited    BOOLEAN   DEFAULT FALSE,

    -- Uniqueness: One role per subject
    UNIQUE (subject_type, subject_id)
);
