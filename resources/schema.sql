-- =========================
-- VAULTHALLA DATABASE SCHEMA
-- =========================
-- Nextcloud replacement, forged by the gods
-- =========================

-- USERS AND ROLES

CREATE TABLE users (
                       id SERIAL PRIMARY KEY,
                       username VARCHAR(150) UNIQUE NOT NULL,
                       email VARCHAR(255) UNIQUE NOT NULL,
                       password_hash TEXT NOT NULL,
                       display_name VARCHAR(255),
                       created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                       last_login TIMESTAMP,
                       is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE roles (
                       id SERIAL PRIMARY KEY,
                       name VARCHAR(100) UNIQUE NOT NULL,
                       description TEXT
);

CREATE TABLE user_roles (
                            user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
                            role_id INTEGER REFERENCES roles(id) ON DELETE CASCADE,
                            PRIMARY KEY (user_id, role_id)
);

CREATE TABLE permissions (
                             id SERIAL PRIMARY KEY,
                             name VARCHAR(100) UNIQUE NOT NULL,
                             description TEXT
);

CREATE TABLE role_permissions (
                                  role_id INTEGER REFERENCES roles(id) ON DELETE CASCADE,
                                  permission_id INTEGER REFERENCES permissions(id) ON DELETE CASCADE,
                                  PRIMARY KEY (role_id, permission_id)
);

-- STORAGE BACKENDS AND VOLUMES

CREATE TABLE storage_backends (
                                  id SERIAL PRIMARY KEY,
                                  name VARCHAR(150) UNIQUE NOT NULL,
                                  type VARCHAR(50) NOT NULL, -- 'local_disk', 's3', 'wasabi', etc
                                  config JSONB NOT NULL,
                                  is_active BOOLEAN DEFAULT TRUE,
                                  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE storage_volumes (
                                 id SERIAL PRIMARY KEY,
                                 storage_backend_id INTEGER REFERENCES storage_backends(id) ON DELETE CASCADE,
                                 name VARCHAR(150) NOT NULL,
                                 path_prefix TEXT,
                                 quota_bytes BIGINT DEFAULT NULL,
                                 created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE user_storage_volumes (
                                      user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
                                      storage_volume_id INTEGER REFERENCES storage_volumes(id) ON DELETE CASCADE,
                                      PRIMARY KEY (user_id, storage_volume_id)
);

-- FILES AND METADATA

CREATE TABLE files (
                       id SERIAL PRIMARY KEY,
                       storage_volume_id INTEGER REFERENCES storage_volumes(id) ON DELETE CASCADE,
                       parent_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
                       name VARCHAR(500) NOT NULL,
                       is_directory BOOLEAN DEFAULT FALSE,
                       created_by INTEGER REFERENCES users(id),
                       created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                       updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                       UNIQUE (storage_volume_id, parent_id, name)
);

CREATE TABLE file_versions (
                               id SERIAL PRIMARY KEY,
                               file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
                               version_number INTEGER NOT NULL,
                               content_hash VARCHAR(128) NOT NULL,
                               size_bytes BIGINT NOT NULL,
                               mime_type VARCHAR(255),
                               storage_path TEXT NOT NULL,
                               created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                               created_by INTEGER REFERENCES users(id),
                               UNIQUE (file_id, version_number)
);

CREATE TABLE file_metadata (
                               id SERIAL PRIMARY KEY,
                               file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
                               key VARCHAR(255) NOT NULL,
                               value TEXT,
                               UNIQUE (file_id, key)
);

CREATE TABLE file_shares (
                             id SERIAL PRIMARY KEY,
                             file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
                             shared_by INTEGER REFERENCES users(id),
                             shared_with INTEGER REFERENCES users(id),
                             permission VARCHAR(50) NOT NULL, -- 'read', 'write', 'owner'
                             expires_at TIMESTAMP,
                             created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FUTURE-PROOF EXTENSIONS / FILE REPRESENTATIONS

CREATE TABLE file_icons (
                            file_id INTEGER PRIMARY KEY REFERENCES files(id) ON DELETE CASCADE,
                            icon_path TEXT,
                            thumbnail_path TEXT,
                            preview_path TEXT,
                            generated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FULL AUDIT LOGGING

CREATE TABLE audit_log (
                           id SERIAL PRIMARY KEY,
                           user_id INTEGER REFERENCES users(id),
                           action VARCHAR(100) NOT NULL,
                           target_file_id INTEGER REFERENCES files(id),
                           timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                           ip_address VARCHAR(50),
                           user_agent TEXT
);

-- FUTURE UPGRADE: GROUPS (TEAM SHARING)

CREATE TABLE groups (
                        id SERIAL PRIMARY KEY,
                        name VARCHAR(150) UNIQUE NOT NULL,
                        description TEXT,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE group_memberships (
                                   group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE,
                                   user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
                                   PRIMARY KEY (group_id, user_id)
);

CREATE TABLE group_storage_volumes (
                                       group_id INTEGER REFERENCES groups(id) ON DELETE CASCADE,
                                       storage_volume_id INTEGER REFERENCES storage_volumes(id) ON DELETE CASCADE,
                                       PRIMARY KEY (group_id, storage_volume_id)
);

-- FUTURE UPGRADE: FILE TAGGING

CREATE TABLE file_tags (
                           id SERIAL PRIMARY KEY,
                           name VARCHAR(100) UNIQUE NOT NULL
);

CREATE TABLE file_tag_assignments (
                                      file_id INTEGER REFERENCES files(id) ON DELETE CASCADE,
                                      tag_id INTEGER REFERENCES file_tags(id) ON DELETE CASCADE,
                                      PRIMARY KEY (file_id, tag_id)
);

-- FUTURE UPGRADE: TRASHED FILES (SOFT DELETE)

CREATE TABLE trashed_files (
                               id SERIAL PRIMARY KEY,
                               file_id INTEGER UNIQUE NOT NULL REFERENCES files(id) ON DELETE CASCADE,
                               trashed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                               trashed_by INTEGER REFERENCES users(id),
                               reason TEXT
);

-- FUTURE UPGRADE: SEARCH INDEX SUPPORT

-- Assuming you add a full-text index layer later for fast search:

CREATE TABLE file_search_index (
                                   file_id INTEGER PRIMARY KEY REFERENCES files(id) ON DELETE CASCADE,
                                   search_vector tsvector -- fill with triggers or background jobs
);

CREATE INDEX file_search_vector_idx ON file_search_index USING GIN (search_vector);

-- =========================
-- END OF MASTER SCHEMA
-- =========================

