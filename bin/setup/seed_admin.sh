#!/bin/bash

HASHED_PASS="$1"

cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Insert Admin User if not exists
INSERT INTO users (name, password_hash, created_at, is_active)
VALUES ('admin', '${HASHED_PASS}', NOW(), TRUE)
ON CONFLICT (name) DO NOTHING;

-- Link to user_role_assignments
INSERT INTO user_role_assignments (user_id, role_id)
SELECT u.id, r.id
FROM users u, role r
WHERE u.name = 'admin' AND r.name = 'super_admin' AND r.type = 'user';

-- Create Admin Group & Link
INSERT INTO groups (name, description)
VALUES ('admin', 'Core admin group')
ON CONFLICT (name) DO NOTHING;

INSERT INTO group_members (group_id, user_id)
SELECT g.id, u.id
FROM groups g, users u
WHERE g.name = 'admin'
AND NOT EXISTS (
  SELECT 1 FROM group_members
  WHERE group_id = g.id AND user_id = u.id
);

-- Create Admin Default Vault if not exists
INSERT INTO vault (type, name, is_active, created_at, owner_id, description)
VALUES ('local', 'Default', TRUE, NOW(), (SELECT id FROM users WHERE name = 'admin'), 'Default vault for admin user')
ON CONFLICT (name) DO NOTHING;

-- Insert local mount point if not exists
INSERT INTO local (vault_id, mount_point)
SELECT v.id, 'users/admin'
FROM vault v
WHERE v.name = 'Default'
AND NOT EXISTS (
  SELECT 1 FROM local WHERE vault_id = v.id
);

-- Seed root directory for admin vault, including directories table
WITH inserted_fs_entry AS (
    INSERT INTO fs_entry (vault_id, name, path, parent_id, created_by, last_modified_by)
    VALUES (
        (SELECT id FROM vault WHERE name = 'Default'),
        '/', '/', NULL,
        (SELECT id FROM users WHERE name = 'admin'),
        (SELECT id FROM users WHERE name = 'admin')
    )
    ON CONFLICT DO NOTHING
    RETURNING id
)
INSERT INTO directories (fs_entry_id)
SELECT id FROM inserted_fs_entry
ON CONFLICT DO NOTHING;
EOF
