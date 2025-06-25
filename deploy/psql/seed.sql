-- SEED DATA FOR VAULTHALLA INSTALLATION

-- Insert roles with permission bitmasks
-- Bitmask is 16 bits, written in binary or as hex
-- Bit positions are 1-indexed, so shift by (bit - 1)

-- SuperAdmin: all 16 bits enabled (0b1111111111111111 = 0xFFFF)
-- Admin: first 10 permissions (0b0000001111111111 = 0x03FF)
-- User: Upload, Download, Share (bits 6, 7, 9 = 0b0000001011000000 = 0x02C0)
-- Guest: Download only (bit 7 = 0b0000000001000000 = 0x0040)

INSERT INTO roles (name, description, permissions)
VALUES
    ('admin', 'Full system administrator with all permissions', B'0000001111111111'),
    ('user', 'Standard user with access to personal files',     B'0000001011000000'),
    ('guest', 'Limited access for shared files',                B'0000000001000000'),
    ('super_admin', 'Root-level internal use only',             B'1111111111111111');

-- Insert permissions with snake_case identifiers
INSERT INTO permissions (bit_position, name, description)
VALUES
    (1, 'manage_users',     'Can manage users and assign roles'),
    (2, 'manage_roles',     'Can manage role definitions and assignments'),
    (3, 'manage_storage',   'Can create and configure storage backends'),
    (4, 'manage_files',     'Can manage files and folders'),
    (5, 'view_audit_log',   'Can view system audit logs'),
    (6, 'upload_file',      'Can upload files'),
    (7, 'download_file',    'Can download files'),
    (8, 'delete_file',      'Can delete files'),
    (9, 'share_file',       'Can create file shares'),
    (10, 'lock_file',       'Can apply locks to files');

