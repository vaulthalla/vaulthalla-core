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
INSERT INTO vault (type, name, mount_point, is_active, created_at, owner_id, description)
VALUES ('local', 'Default', 'users/admin', TRUE, NOW(), (SELECT id FROM users WHERE name = 'admin'), 'Default vault for admin user')
ON CONFLICT (name) DO NOTHING;

-- Create sync entry for Default Vault
WITH ins AS (
    INSERT INTO sync (vault_id, interval)
    VALUES(
        (SELECT id FROM vault WHERE name = 'Default'),
        600
    ) RETURNING id
)
INSERT INTO fsync (sync_id, conflict_policy)
SELECT id, 'keep_both' FROM ins;

-- Seed root directory
WITH inserted_fs_entry AS (
    INSERT INTO fs_entry (name, path, fuse_path, inode, mode, parent_id, created_by, last_modified_by)
    VALUES (
        '/', '/', '/', 1, 0755, NULL,
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
