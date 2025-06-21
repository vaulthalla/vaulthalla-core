-- SEED DATA FOR VAULTHALLA INSTALLATION

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
