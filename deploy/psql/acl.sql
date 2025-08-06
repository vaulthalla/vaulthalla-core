-- ##################################
--- Access Control List (ACL) Schema
-- ##################################

CREATE TABLE permission
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) NOT NULL,
    description  TEXT,
    category     VARCHAR(12)        NOT NULL CHECK (category IN ('user', 'vault')),
    bit_position INTEGER            NOT NULL CHECK (bit_position >= 0 AND bit_position < 64),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (name, category) -- Uniqueness: One permission per name-category pair
);

CREATE TABLE role
(
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(50) UNIQUE NOT NULL,
    type        VARCHAR(12)        NOT NULL CHECK (type IN ('user', 'vault')),
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ##################################
--- Role Definitions
-- ##################################

CREATE TABLE permissions
(
    id          SERIAL PRIMARY KEY,
    role_id     INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    permissions BIT(16) NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (role_id) -- Uniqueness: One set of permissions per role
);

-- ###########################################
--- Role Assignments
-- ###########################################

CREATE TABLE user_role_assignments
(
    id          SERIAL PRIMARY KEY,
    user_id     INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    role_id     INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (user_id, role_id) -- Uniqueness: One role per user
);

CREATE TABLE vault_role_assignments
(
    id           SERIAL PRIMARY KEY,
    vault_id     INTEGER     NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    subject_type VARCHAR(10) NOT NULL CHECK (subject_type IN ('user', 'group')),
    subject_id   INTEGER     NOT NULL,
    role_id      INTEGER     NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    assigned_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (vault_id, subject_type, subject_id) -- Uniqueness: One role per vault-subject pair
);

-- ###########################################
--- Permission Overrides
-- ###########################################

CREATE TABLE vault_permission_overrides
(
    id            SERIAL PRIMARY KEY,
    assignment_id INTEGER NOT NULL REFERENCES vault_role_assignments (id) ON DELETE CASCADE,
    permission_id INTEGER     NOT NULL REFERENCES permission (id) ON DELETE CASCADE,
    scope         VARCHAR(10) NOT NULL CHECK (scope IN ('vault', 'file', 'directory')),
    enabled       BOOLEAN     DEFAULT FALSE, -- Sets whether the permission is enabled or not
    regex         TEXT        NOT NULL,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (assignment_id, permission_id, regex) -- Uniqueness: One override per role-permission-regex combination
);

-- ###########################################
--- Indexes
-- ###########################################

CREATE UNIQUE INDEX idx_permission_name_category
    ON permission (LOWER(name), category);

CREATE INDEX idx_vault_role_assignments_vault_subject
    ON vault_role_assignments (vault_id, subject_type, subject_id);

CREATE INDEX idx_vault_permission_overrides_perm
    ON vault_permission_overrides (permission_id);

CREATE INDEX idx_vault_permission_overrides_assignment_scope
    ON vault_permission_overrides (assignment_id, scope);
