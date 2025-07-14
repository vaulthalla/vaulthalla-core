#!/bin/bash

cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Cloud Test Vault and keys
INSERT INTO api_keys (name, user_id, type, created_at)
VALUES ('R2 Test Key', (SELECT id FROM users WHERE name = 'admin'), 's3', NOW())
ON CONFLICT (name) DO NOTHING;

INSERT INTO s3_api_keys (api_key_id, provider, access_key, secret_access_key, region, endpoint)
SELECT
  (SELECT id FROM api_keys WHERE name = 'R2 Test Key'),
  'Cloudflare R2',
  '${VAULTHALLA_TEST_R2_ACCESS_KEY}',
  '${VAULTHALLA_TEST_R2_SECRET_ACCESS_KEY}',
  'wnam',
  '${VAULTHALLA_TEST_R2_ENDPOINT}'
WHERE NOT EXISTS (
  SELECT 1 FROM s3_api_keys WHERE api_key_id = (SELECT id FROM api_keys WHERE name = 'R2 Test Key')
);

-- Create test vault for R2
INSERT INTO vault (type, name, is_active, created_at, owner_id, description)
VALUES (
    's3', 'R2 Test Vault', TRUE, NOW(),
    (SELECT id FROM users WHERE name = 'admin'),
    'Test vault for Cloudflare R2 integration'
)
ON CONFLICT (name) DO NOTHING;

-- Create sync entry for R2 Test Vault
WITH inserted_sync AS (
  INSERT INTO sync (interval, conflict_policy, strategy)
  VALUES (60, 'keep_local', 'sync')
  RETURNING id
)
INSERT INTO proxy_sync (vault_id, sync_id)
SELECT v.id, i.id
FROM vault v, inserted_sync i
WHERE v.name = 'R2 Test Vault';

-- Create test bucket for R2
INSERT INTO s3_buckets (name, api_key_id)
VALUES (
    'vaulthalla-test',
    (SELECT id FROM api_keys WHERE name = 'R2 Test Key')
);

-- Insert s3 mount point if not exists
INSERT INTO s3 (vault_id, bucket_id)
SELECT v.id, b.id
FROM vault v, s3_buckets b
WHERE v.name = 'R2 Test Vault'
AND b.name = 'vaulthalla-test'
AND NOT EXISTS (
  SELECT 1 FROM s3 WHERE vault_id = v.id AND bucket_id = b.id
);
EOF
