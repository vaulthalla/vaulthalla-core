# ⚡️ Vaulthalla | The Final Cloud ⚡️

**Military-grade encryption. Lightning-fast performance.**  
**Your files, your terms, forever.**

---

**No plugins. No PHP. No compromises.**  
**Storage distilled to perfection.**

## Why Vaulthalla?

Vaulthalla is engineered from scratch for speed, sovereignty, and unyielding security:

| Feature                  | Description                                                                                         |
|--------------------------|-----------------------------------------------------------------------------------------------------|
| ⚙️ **Compiled Core**      | C++23 codebase, zero runtime overhead. Built for raw performance.                                   |
| 🔄 **True FUSE Mounts**   | Filesystem integration via libfuse3. Your cloud, your `/mnt`.                                       |
| 🔐 **AES256-GCM/NI**      | libsodium-backed encryption with hardware AES-NI acceleration.                                      |
| 🧱 **TPM2 Key Sealing**   | All encryption keys sealed via `tpm2-tss`, never stored in plaintext.                               |
| 💾 **PostgreSQL Backbone**| Transactional, ACID-compliant metadata persistence.                                                  |
| 🚫 **No Docker Needed**   | Built Debian-first. No containers required to go live.                                               |
| ☁️ **S3 Compatible**      | Sync and mirror to AWS, MinIO, R2, and any S3-compatible endpoint.                                   |
| 🔄 **Zero Trust Sync**    | Local-to-cloud sync with enforced permissions and sealed metadata.                                  |

## ☁️ Intelligent Synchronization

Three flexible strategies for managing storage:

* **⚡ Smart Cache:** Lazy downloads, automatic eviction, and disk-aware operation.
* **🔄 Two-Way Sync:** Local and cloud parity. Robust conflict resolution.
* **🪞 Mirror Mode:** One-direction replication, perfect for backup or cold storage.

## 🔐 Security by Design

Everything encrypted. Nothing assumed. Vaulthalla enforces best practices out of the box:

* AES-256-GCM file encryption with libsodium
* TPM2-sealed symmetric keys using `tpm2-tss`
* Role-based access control and permission bitmasks
* Password hash hardening and live dictionary blacklisting
* Encrypted API secrets and key metadata
* JWT-secured sessions

---

## 🚀 Quick Installation (Development Mode)

Vaulthalla is under active development. Expect frequent updates. For local testing:

```bash
git clone https://github.com/vaulthalla/server.git
cd server
make install -- -d
```

The `-d` flag enables developer mode:

* Debug build
* Auto-created admin user (`vh!adm1n`)
* Verbose logging
* Dev vaults and Cloudflare R2 S3 test setup

**⚠️ Warning:** Dev mode will reset your database and overwrite any existing Vaulthalla configs.

---

## ✅ Verifying Installation

```bash
systemctl status vaulthalla-core vaulthalla-fuse
journalctl -f -u vaulthalla-core
```

---

## ⚠️ Considerations

* Port 443 must be open for HTTPS.
* Default config lives in `/etc/vaulthalla/config.yaml`.
* Back up encryption keys and database state regularly.

---

## 💡 Support & Contribution

We welcome contributions, issue reports, and feedback. Contributor interest form coming soon.

---

## 🚧 Development Notes

Full architecture documentation coming soon.

---

### Mission Statement

**Vaulthalla is for those who refuse to rent back their own data.**

No subscriptions. No surveillance. No bloat. Just one battle-forged binary—hardened for performance, encrypted like state secrets, and mounted directly into your filesystem.

This is storage as it should be: fast, sovereign, and truly yours.
