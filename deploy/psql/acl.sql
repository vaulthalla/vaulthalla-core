CREATE TABLE permission
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
    simple_permissions    BOOLEAN DEFAULT FALSE, -- If true, only simple permissions are used
    created_at            TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE simple_permissions
(
    id           SERIAL PRIMARY KEY,
    role_id      INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    permissions  BIT(16) NOT NULL,
    UNIQUE (role_id) -- Ensure only one set of simple permissions per role
);

CREATE TABLE permissions
(
    id           SERIAL PRIMARY KEY,
    role_id      INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    file_permissions: BIT(16) NOT NULL,
    directory_permissions: BIT(16) NOT NULL,
    UNIQUE (role_id)  -- Ensure only one set of advanced permissions per role
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


-- View to summarize role permissions
CREATE VIEW role_permission_summary AS
SELECT r.id AS role_id,
       (sp.id IS NOT NULL) AS has_simple,
       (p.id IS NOT NULL) AS has_advanced
FROM role r
         LEFT JOIN simple_permissions sp ON sp.role_id = r.id
         LEFT JOIN permissions p ON p.role_id = r.id;


-- Trigger function to check role permission exclusivity
CREATE OR REPLACE FUNCTION check_role_permission_exclusivity()
RETURNS TRIGGER AS $$
BEGIN
  IF EXISTS (
    SELECT 1 FROM role_permission_summary
    WHERE role_id = NEW.role_id
    AND ((has_simple AND has_advanced) OR (NOT has_simple AND NOT has_advanced))
  ) THEN
    RAISE EXCEPTION 'Role % must have either simple OR advanced permissions, not both or neither', NEW.role_id;
END IF;

RETURN NEW;
END;
$$ LANGUAGE plpgsql;


-- Triggers to enforce the exclusivity of simple and advanced permissions
CREATE TRIGGER check_simple_permissions
    AFTER INSERT OR DELETE ON simple_permissions
FOR EACH ROW EXECUTE FUNCTION check_role_permission_exclusivity();

CREATE TRIGGER check_advanced_permissions
    AFTER INSERT OR DELETE ON permissions
FOR EACH ROW EXECUTE FUNCTION check_role_permission_exclusivity();

