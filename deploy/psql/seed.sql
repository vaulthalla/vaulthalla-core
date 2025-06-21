-- SEED DATA FOR VAULTHALLA INSTALLATION

DEFAULT_PASS="vh!adm1n"
HASH_TOOL="./hash_password"
ADMIN_EMAIL="admin@vaulthalla.lan"
ADMIN_NAME="Admin"
VAUL_DB="vaulthalla"
VAUL_USER="vaulthalla"

-- Insert roles
INSERT INTO roles (name, description) VALUES
                                          ('Admin', 'Full system administrator with all permissions'),
                                          ('User', 'Standard user with access to personal files'),
                                          ('Guest', 'Limited access for shared files'),
                                          ('Moderator', 'Can manage public shares and audit logs'),
                                          ('SuperAdmin', 'Root-level internal use only');

-- Insert permissions
INSERT INTO permissions (name, description) VALUES
                                                ('ManageUsers', 'Can manage users and assign roles'),
                                                ('ManageRoles', 'Can manage role definitions and assignments'),
                                                ('ManageStorage', 'Can create and configure storage backends'),
                                                ('ManageFiles', 'Can manage files and folders'),
                                                ('ViewAuditLog', 'Can view system audit logs'),
                                                ('UploadFile', 'Can upload files'),
                                                ('DownloadFile', 'Can download files'),
                                                ('DeleteFile', 'Can delete files'),
                                                ('ShareFile', 'Can create file shares'),
                                                ('LockFile', 'Can apply locks to files');

-- Create initial admin user
# Prompt for password
echo -n "Enter admin password (or press Enter for default): "
read -rs USER_PASS
echo

# Use default if blank
if [[ -z "$USER_PASS" ]]; then
    echo "⚠️  Using default admin password: $DEFAULT_PASS"
    USER_PASS="$DEFAULT_PASS"
fi

# Hash it
HASHED=$($HASH_TOOL "$USER_PASS")
if [[ $? -ne 0 || -z "$HASHED" ]]; then
    echo "❌ Password hashing failed."
    exit 1
fi

# Construct SQL insert
SQL=$(cat <<EOF
INSERT INTO users (name, email, password_hash, created_at, is_active)
VALUES ('$ADMIN_NAME', '$ADMIN_EMAIL', '$HASHED', NOW(), TRUE);
EOF
)

# Run the insert
echo "🚀 Inserting admin user into database..."
echo "$SQL" | psql -U $VAUL_USER -d $VAUL_DB

if [[ $? -eq 0 ]]; then
    echo "✅ Admin user seeded successfully!"
else
    echo "❌ Failed to insert admin user."
    exit 1
fi

-- Assign admin role to admin user
INSERT INTO user_roles (user_id, role_id)
SELECT users.id, roles.id FROM users, roles
WHERE users.email = 'admin@vaulthalla.lan' AND roles.name = 'Admin';

-- Create admin group
INSERT INTO groups (name, description) VALUES ('admin', 'Core admin group');

-- Add admin to admin group
INSERT INTO group_members (gid, uid)
SELECT groups.id, users.id FROM users, groups
WHERE users.email = 'admin@vaulthalla.lan' AND groups.name = 'admin';

-- Create vault
INSERT INTO vaults (type, name, is_active, created_at)
VALUES ('LocalDisk', 'Admin Local Disk Vault', TRUE, '2025-06-21 13:59:49');

-- Link vault to local disk
INSERT INTO local_disk_vaults (vault_id, mount_point)
SELECT id, '/mnt/vaulthalla/users/admin' FROM vaults WHERE name = 'Admin Local Disk Vault';

-- Create a storage volume for admin user
INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes, created_at)
SELECT id, 'Admin Local Disk Vault', '/users/admin', NULL, '2025-06-21 13:59:49' FROM vaults WHERE name = 'Admin Local Disk Vault';

-- Link admin user to storage volume
INSERT INTO user_storage_volumes (user_id, storage_volume_id)
SELECT users.id, storage_volumes.id
FROM users, storage_volumes
WHERE users.email = 'admin@vaulthalla.lan' AND storage_volumes.name = 'Admin Local Disk Vault';

-- Assign all permissions to Admin role globally
INSERT INTO role_permissions (role_id, permission_id)
SELECT roles.id, permissions.id FROM roles, permissions
WHERE roles.name = 'Admin';
