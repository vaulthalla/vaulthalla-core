-- SEED DATA FOR VAULTHALLA INSTALLATION

-- Insert roles
INSERT INTO roles (name, description) VALUES
                                          ('Admin', 'Full system administrator with all permissions'),
                                          ('User', 'Standard user with access to personal files'),
                                          ('Guest', 'Limited access for shared files'),
                                          ('SuperAdmin', 'Root-level internal use only');

-- Insert permissions
INSERT INTO permissions (bit_position, name, description) VALUES
                                                (1, 'ManageUsers', 'Can manage users and assign roles'),
                                                (2, 'ManageRoles', 'Can manage role definitions and assignments'),
                                                (3, 'ManageStorage', 'Can create and configure storage backends'),
                                                (4, 'ManageFiles', 'Can manage files and folders'),
                                                (5, 'ViewAuditLog', 'Can view system audit logs'),
                                                (6, 'UploadFile', 'Can upload files'),
                                                (7, 'DownloadFile', 'Can download files'),
                                                (8, 'DeleteFile', 'Can delete files'),
                                                (9, 'ShareFile', 'Can create file shares'),
                                                (10, 'LockFile', 'Can apply locks to files');
