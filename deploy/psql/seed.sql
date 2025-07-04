-- SEED DATA FOR VAULTHALLA INSTALLATION
-- --------------------------------------
-- Default roles and permissions for fresh deployments

-- ROLE DEFINITIONS
-- Super Admin: All permissions enabled
-- Admin: All except CreateAdminUser (bit 1 disabled)
-- PowerUser: Vault + File + Directory full control, no admin perms
-- User: Basic file + dir operations
-- Guest: Read-only

INSERT INTO role
(name, description, simple_permissions, created_at)
VALUES
-- ────────────────────────────────────────────────────────────────────────────────
('super_admin', 'Root-level system owner with unrestricted access', true, NOW()),
('admin', 'System administrator with all non-root administrative powers', true, NOW()),
('power_user', 'Advanced user with full file/vault control but no admin authority', true, NOW()),
('user', 'Standard user with basic file operations', true, NOW()),
('guest', 'Minimal access: can download files and list directories', true, NOW());

-- PERMISSION DEFINITIONS
INSERT INTO permission (bit_position, name, description, category)
VALUES
-- Admin
(0, 'create_user', 'Can create standard users', 'admin'),
(1, 'create_admin_user', 'Can create admin users', 'admin'),
(2, 'deactivate_user', 'Can deactivate user accounts', 'admin'),
(3, 'reset_user_password', 'Can reset user passwords', 'admin'),
(4, 'manage_roles', 'Can manage role definitions and assignments', 'admin'),
(5, 'manage_settings', 'Can change system settings', 'admin'),
(6, 'view_audit_log', 'Can view system audit logs', 'admin'),
(7, 'manage_api_keys', 'Can create and manage API keys', 'admin'),
(8, 'create_local_vault', 'Can create local vaults', 'admin'),
(9, 'create_cloud_vault', 'Can create cloud vaults', 'admin'),
(10, 'delete_vault', 'Can delete vaults', 'admin'),
(11, 'manage_vault_settings', 'Can modify vault settings', 'admin'),
(12, 'manage_vault_roles', 'Can manage vault role assignments', 'admin'),
(13, 'migrate_vault_data', 'Can initiate vault data migrations', 'admin'),
(14, 'manage_all_vaults', 'Can manage all vaults', 'admin'),

-- FS (File + Directory shared)
(0, 'upload', 'Can upload files or directories', 'fs'),
(1, 'download', 'Can download files or directories', 'fs'),
(2, 'delete', 'Can delete files or directories', 'fs'),
(3, 'share_public', 'Can share files or directories publicly', 'fs'),
(4, 'share_internal', 'Can share files or directories internally', 'fs'),
(5, 'lock', 'Can apply locks to files or directories', 'fs'),
(6, 'rename', 'Can rename files or directories', 'fs'),
(7, 'move', 'Can move files or directories', 'fs'),
(8, 'sync_local', 'Can sync files or directories locally', 'fs'),
(9, 'sync_cloud', 'Can sync files or directories to cloud', 'fs'),
(10, 'modify_metadata', 'Can modify metadata on files or directories', 'fs'),
(11, 'change_icons', 'Can change icons on files or directories', 'fs'),
(12, 'manage_tags', 'Can manage tags on files or directories', 'fs'),
(13, 'list', 'Can list contents of directories', 'fs'),
(14, 'manage_versions', 'Can manage file versions', 'fs');
