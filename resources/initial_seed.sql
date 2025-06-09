-- =============================
-- VAULTHALLA INITIAL SEED SCRIPT
-- =============================

-- ROLES

INSERT INTO roles (name, description)
VALUES
    ('admin', 'Administrator with full system permissions'),
    ('user', 'Standard user with basic file operations'),
    ('auditor', 'Can view audit logs and file metadata only');

-- PERMISSIONS (OPTIONAL EXAMPLE)

INSERT INTO permissions (name, description)
VALUES
    ('manage_users', 'Create, delete, and manage users'),
    ('manage_roles', 'Create, delete, and manage roles and permissions'),
    ('manage_storage', 'Configure storage backends and volumes'),
    ('manage_files', 'Perform all file operations, including permanent delete'),
    ('view_audit_log', 'View system-wide audit logs');

-- MAP ADMIN ROLE TO ALL PERMISSIONS

INSERT INTO role_permissions (role_id, permission_id)
SELECT
    (SELECT id FROM roles WHERE name = 'admin'),
    p.id
FROM permissions p;

-- CREATE ADMIN USER

INSERT INTO users (username, email, password_hash, display_name, is_active)
VALUES
    ('admin', 'admin@vaulthalla.local', '$argon2id$v=19$m=65536,t=3,p=4$REPLACE_WITH_REAL_HASH', 'Vaulthalla Admin', TRUE);

-- MAP ADMIN USER TO ADMIN ROLE

INSERT INTO user_roles (user_id, role_id)
VALUES
    (
        (SELECT id FROM users WHERE username = 'admin'),
        (SELECT id FROM roles WHERE name = 'admin')
    );

-- STORAGE BACKENDS EXAMPLES

-- Local Disk

INSERT INTO storage_backends (name, type, config, is_active)
VALUES
    (
        'LocalDisk_Main',
        'local_disk',
        '{"mount_point": "/mnt/vaulthalla/local_main"}',
        TRUE
    );

-- S3 Example

INSERT INTO storage_backends (name, type, config, is_active)
VALUES
    (
        'AWS_S3_Prod',
        's3',
        '{
            "bucket_name": "vaulthalla-prod-bucket",
            "region": "us-west-2",
            "access_key_id": "REPLACE_ME",
            "secret_access_key": "REPLACE_ME",
            "prefix": "vaulthalla/"
        }',
        TRUE
    );

-- STORAGE VOLUMES

-- Admin personal volume on LocalDisk

INSERT INTO storage_volumes (storage_backend_id, name, path_prefix, quota_bytes)
VALUES
    (
        (SELECT id FROM storage_backends WHERE name = 'LocalDisk_Main'),
        'Admin_Private',
        'admin_private/',
        107374182400 -- 100 GiB
    );

-- MAP ADMIN TO ADMIN_PRIVATE VOLUME

INSERT INTO user_storage_volumes (user_id, storage_volume_id)
VALUES
    (
        (SELECT id FROM users WHERE username = 'admin'),
        (SELECT id FROM storage_volumes WHERE name = 'Admin_Private')
    );

-- STORAGE VOLUME for future group mapping (optional)

INSERT INTO storage_volumes (storage_backend_id, name, path_prefix, quota_bytes)
VALUES
    (
        (SELECT id FROM storage_backends WHERE name = 'AWS_S3_Prod'),
        'Shared_Team',
        'team_shared/',
        NULL
    );

-- =============================
-- SEED COMPLETE
-- =============================

