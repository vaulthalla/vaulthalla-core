-- SEED DATA FOR VAULTHALLA INSTALLATION
-- --------------------------------------
-- Default roles and permissions for fresh deployments

-- ROLE DEFINITIONS
INSERT INTO role (name, description, type, created_at)
VALUES ('super_admin', 'Root-level system owner with unrestricted access', 'user', NOW()),
       ('admin', 'System administrator with all non-root administrative powers', 'user', NOW()),
       ('power_user', 'Advanced user with full vault control but no admin authority', 'vault', NOW()),
       ('user', 'Standard user with basic file operations', 'vault', NOW()),
       ('guest', 'Minimal access: can download files and list directories', 'vault', NOW());

-- ROLE PERMISSIONS
-- Admin roles
INSERT INTO permissions (role_id, permissions) VALUES
    -- Super Admin: all admin perms (bit 0-6 all on)
    ((SELECT id FROM role WHERE name = 'super_admin'), B'0000000001111111'),

    -- Admin: all admin perms except manage_admins (bit 0 = 0, bits 1-6 = 1)
    ((SELECT id FROM role WHERE name = 'admin'), B'0000000001111110');

-- Vault roles
INSERT INTO permissions (role_id, permissions) VALUES
    -- Power User: all vault perms
    ((SELECT id FROM role WHERE name = 'power_user'), B'0011111111111111'),

    -- User: basic create, download, rename, move, list
    ((SELECT id FROM role WHERE name = 'user'), B'0000000111101000'),

    -- Guest: download + list only
    ((SELECT id FROM role WHERE name = 'guest'), B'0000000000101000');

-- PERMISSION DEFINITIONS
INSERT INTO permission (bit_position, name, description, category)
VALUES
-- Admin bits (user category)
(0, 'manage_admins', 'Can manage admin users (create, deactivate, modify)', 'user'),
(1, 'manage_users', 'Can manage regular users', 'user'),
(2, 'manage_roles', 'Can create, modify, delete roles', 'user'),
(3, 'manage_settings', 'Can modify system-wide settings', 'user'),
(4, 'manage_vaults', 'Can create, delete, modify any vault settings', 'user'),
(5, 'access_audit_logs', 'Can view system audit logs', 'user'),
(6, 'manage_api_keys', 'Can manage API keys globally', 'user'),

-- Vault bits (vault category)
(0, 'migrate_data', 'Can migrate vault data to another storage backend', 'vault'),
(1, 'manage_access', 'Can manage vault roles and access rules', 'vault'),
(2, 'manage_tags', 'Can manage tags for files and directories', 'vault'),
(3, 'manage_metadata', 'Can manage file and directory metadata', 'vault'),
(4, 'manage_versions', 'Can manage file version history', 'vault'),
(5, 'manage_file_locks', 'Can lock or unlock files', 'vault'),
(6, 'share', 'Can create public sharing links', 'vault'),
(7, 'sync', 'Can sync vault data to external/cloud storage', 'vault'),
(8, 'create', 'Can create files or directories and upload files', 'vault'),
(9, 'download', 'Can download files or read file contents', 'vault'),
(10, 'delete', 'Can delete files or directories', 'vault'),
(11, 'rename', 'Can rename files or directories', 'vault'),
(12, 'move', 'Can move files or directories', 'vault'),
(13, 'list', 'Can list directory contents', 'vault');
