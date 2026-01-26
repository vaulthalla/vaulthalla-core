-- ##################################
--- Access Control List (ACL) Schema
-- ##################################

-- ACL base tables (must run before acl assignments/overrides)

CREATE TABLE IF NOT EXISTS permission
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) NOT NULL,
    description  TEXT,
    category     VARCHAR(12) NOT NULL CHECK (category IN ('user', 'vault')),
    bit_position INTEGER NOT NULL CHECK (bit_position >= 0 AND bit_position < 64),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (name, category)
);

CREATE TABLE IF NOT EXISTS role
(
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(50) UNIQUE NOT NULL,
    type        VARCHAR(12) NOT NULL CHECK (type IN ('user', 'vault')),
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
