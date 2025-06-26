CREATE TABLE permissions
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) UNIQUE NOT NULL,
    display_name VARCHAR(50) GENERATED ALWAYS AS (
        initcap(replace(name, '_', ' '))
        ) STORED,
    description  TEXT,
    bit_position INTEGER UNIQUE     NOT NULL CHECK (bit_position >= 0 AND bit_position < 16),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE role
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) UNIQUE NOT NULL,
    display_name VARCHAR(50) GENERATED ALWAYS AS (
        initcap(replace(name, '_', ' '))
        ) STORED,
    description  TEXT,
    permissions  BIT(16), -- Bitmask for role permissions
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE roles
(
    id           SERIAL PRIMARY KEY,
    subject_type VARCHAR(10) NOT NULL CHECK (subject_type IN ('user', 'group')),
    subject_id   INTEGER     NOT NULL,
    role_id      INTEGER     NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    scope        VARCHAR(10) NOT NULL CHECK (scope IN ('global', 'vault', 'volume')),
    scope_id     INTEGER,
    assigned_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    inherited    BOOLEAN   DEFAULT FALSE,

    -- Uniqueness: One role per subject per scope target
    UNIQUE (subject_type, subject_id, scope, scope_id)
);

CREATE TABLE file_acl
(
    id           SERIAL PRIMARY KEY,           -- <-- NEW surrogate PK
    file_id      INTEGER REFERENCES files (id) ON DELETE CASCADE,
    subject_type TEXT CHECK (subject_type IN ('user', 'group', 'role', 'public')),
    subject_id   INTEGER,                      -- can be NULL if public (enforced via check later)
    permissions  BIT(16) NOT NULL,             -- Match bitset layout
    inherited    BOOLEAN DEFAULT FALSE,
    UNIQUE (file_id, subject_type, subject_id) -- <-- still enforce uniqueness
);

CREATE TABLE file_acl_index
(
    file_id           INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    has_override_acl  BOOLEAN   DEFAULT FALSE,
    subtree_acl_count INTEGER   DEFAULT 0,
    inherited_from_id INTEGER,
    full_path         TEXT,
    last_updated      TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);


CREATE INDEX idx_acl_index_path ON file_acl_index (full_path);

-- This ensures a user can have only ONE role where scope = 'global'
CREATE UNIQUE INDEX uniq_user_global_role
    ON roles (subject_id) WHERE subject_type = 'user' AND scope = 'global';


CREATE FUNCTION enforce_mandatory_global_user_role() RETURNS TRIGGER AS $$
BEGIN
    IF
TG_OP IN ('DELETE', 'UPDATE') AND OLD.subject_type = 'user' AND OLD.scope = 'global' THEN
        IF NOT EXISTS (
            SELECT 1 FROM roles
            WHERE subject_type = 'user'
              AND subject_id = OLD.subject_id
              AND scope = 'global'
              AND id != OLD.id
        ) THEN
            RAISE EXCEPTION USING
                MESSAGE = 'User must always have at least one global role.';
END IF;
END IF;

RETURN NEW;
END;
$$
LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_mandatory_global_user_role
    BEFORE DELETE OR
UPDATE ON roles
    FOR EACH ROW EXECUTE FUNCTION enforce_mandatory_global_user_role();


CREATE FUNCTION validate_subject_role_scope() RETURNS TRIGGER AS $$
BEGIN
    IF
NEW.scope = 'vault' THEN
        IF NOT EXISTS (
            SELECT 1 FROM vaults WHERE id = NEW.scope_id
        ) THEN
            RAISE EXCEPTION USING
                MESSAGE = 'Invalid vault ID in roles.';
END IF;

    ELSIF
NEW.scope = 'volume' THEN
        IF NOT EXISTS (
            SELECT 1 FROM storage_volumes WHERE id = NEW.scope_id
        ) THEN
            RAISE EXCEPTION USING
                MESSAGE = 'Invalid volume ID in roles.';
END IF;

    ELSIF
NEW.scope = 'global' THEN
        IF NEW.scope_id IS NOT NULL THEN
            RAISE EXCEPTION USING
                MESSAGE = 'Global-scoped roles must not define scope_id.';
END IF;
END IF;

RETURN NEW;
END;
$$
LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_subject_role_scope
    BEFORE INSERT OR
UPDATE ON roles
    FOR EACH ROW EXECUTE FUNCTION validate_subject_role_scope();
