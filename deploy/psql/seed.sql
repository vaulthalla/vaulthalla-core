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
(name, description,
 admin_permissions, vault_permissions, file_permissions, directory_permissions, created_at)
VALUES
-- ────────────────────────────────────────────────────────────────────────────────
('super_admin', 'Root-level system owner with unrestricted access',
 X'00FF'::bit(16),  -- 8 bits (all admin perms)
 X'03FF'::bit(16),  -- 10 bits (all vault perms)
 X'00FF'::bit(16),  -- 8 bits (all file perms)
 X'001F'::bit(16),  -- 5 bits (all dir perms)
 NOW()),

('admin', 'System administrator with all non-root administrative powers',
 X'00FD'::bit(16),  -- all admin perms except CreateAdminUser (bit 1 off)
 X'03FF'::bit(16),
 X'00FF'::bit(16),
 X'001F'::bit(16),
 NOW()),

('power_user', 'Advanced user with full file/vault control but no admin authority',
 X'0000'::bit(16),
 X'03FF'::bit(16),
 X'00FF'::bit(16),
 X'001F'::bit(16),
 NOW()),

('user', 'Standard user with basic file operations',
 X'0000'::bit(16),
 X'0000'::bit(16),
 X'00C3'::bit(16),  -- upload, download, share public/group
 X'001F'::bit(16),
 NOW()),

('guest', 'Minimal access: can download files and list directories',
 X'0000'::bit(16),
 X'0000'::bit(16),
 X'0002'::bit(16),  -- download only
 X'0010'::bit(16),
 NOW());

-- PERMISSION DEFINITIONS
INSERT INTO permissions (bit_position, name, description, category)
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

-- Vault
(0, 'create_local_vault', 'Can create local vaults', 'vault'),
(1, 'create_cloud_vault', 'Can create cloud vaults', 'vault'),
(2, 'delete_vault', 'Can delete vaults', 'vault'),
(3, 'adjust_vault_settings', 'Can modify vault settings', 'vault'),
(4, 'migrate_vault_data', 'Can initiate vault data migrations', 'vault'),
(5, 'create_volume', 'Can create volumes', 'vault'),
(6, 'delete_volume', 'Can delete volumes', 'vault'),
(7, 'resize_volume', 'Can resize volumes', 'vault'),
(8, 'move_volume', 'Can move volumes', 'vault'),
(9, 'assign_volume_to_group', 'Can assign volumes to groups', 'vault'),

-- File
(0, 'upload_file', 'Can upload files', 'file'),
(1, 'download_file', 'Can download files', 'file'),
(2, 'delete_file', 'Can delete files', 'file'),
(3, 'share_file_publicly', 'Can share files publicly', 'file'),
(4, 'share_file_with_group', 'Can share files with specific groups', 'file'),
(5, 'lock_file', 'Can apply locks to files', 'file'),
(6, 'rename_file', 'Can rename files', 'file'),
(7, 'move_file', 'Can move files', 'file'),

-- Directory
(0, 'create_directory', 'Can create directories', 'directory'),
(1, 'delete_directory', 'Can delete directories', 'directory'),
(2, 'rename_directory', 'Can rename directories', 'directory'),
(3, 'move_directory', 'Can move directories', 'directory'),
(4, 'list_directory', 'Can list directory contents', 'directory');
