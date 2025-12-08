# ⚡️ Vaulthalla | The Final Cloud ⚡️

**Military-grade encryption. Lightning-fast performance.**  
**Your files, your terms.**

---

## ⚠️ Read This First (Seriously)

Vaulthalla is **not** a user-space toy or a weekend Docker container.

- ✅ **Requires `sudo` / root access** to install and operate  
- ✅ **Manages its own PostgreSQL instance** (schemas, users, migrations)  
- ✅ **Mounts a privileged FUSE filesystem backed by `/var/lib` at `/mnt/vaulthalla`**
- 🧱 **Seals multiple non-exportable TPM2 keys** for encryption, identity, and internal trust domains  
  (keys are hardware-bound and cannot be recovered if the host or TPM state is lost)
- ⚠️ **Developer mode (`-d`) is destructive by design**

If you are not testing inside a **sandbox, VM, or disposable environment**,  
**do not use developer mode.**

This system assumes it owns the machine it’s installed on.

---

## Why Vaulthalla?

Vaulthalla is engineered from first principles for **speed, sovereignty, and uncompromising security**:

| Feature                     | Description                                                                 |
|----------------------------|-----------------------------------------------------------------------------|
| ⚙️ **Compiled Core**        | C++23 codebase. Zero runtime overhead. No interpreters.                     |
| 🔄 **True FUSE Mounts**     | Native filesystem integration via libfuse3. Your cloud, your `/mnt`.        |
| 🔐 **AES-256-GCM / AES-NI** | libsodium encryption with hardware acceleration.                            |
| 🧱 **TPM2 Key Sealing**     | Encryption keys sealed via `tpm2-tss`. Never stored plaintext.              |
| 💾 **PostgreSQL Backbone** | ACID-compliant metadata with transactional guarantees.                      |
| 🚫 **No Docker Required**  | Debian-first. No containers, no indirection.                                |
| ☁️ **S3 Compatible**       | AWS, MinIO, R2, and any S3-compatible endpoint.                             |
| 🔄 **Zero-Trust Sync**     | Permission-aware sync with sealed metadata and enforced policy.             |

---

## ☁️ Intelligent Synchronization

Choose the strategy that fits your threat model and storage philosophy:

- **⚡ Smart Cache** — Lazy fetch, automatic eviction, disk-aware behavior  
- **🔄 Two-Way Sync** — Strong conflict resolution, local ↔ cloud parity  
- **🪞 Mirror Mode** — One-way replication for backups and cold storage  

---

## 🔐 Security by Design

Nothing optional. Nothing implied.

- AES-256-GCM file encryption (libsodium)
- TPM2-sealed symmetric keys
- Role-based access control with permission bitmasks
- Hardened password hashing + live dictionary blacklisting
- Encrypted API secrets and metadata
- JWT-secured sessions

---

## 🚀 Installation

Vaulthalla can be installed either via the official Debian/Ubuntu package
(recommended for most users) or built from source for development.

---

### Option 1: Debian / Ubuntu Package (Recommended)

> ⚠️ **Root access required**  
> The package installs system services, manages PostgreSQL, and mounts a
> privileged FUSE filesystem.

    sudo curl -fsSL https://apt.vaulthalla.sh/pubkey.gpg \
      -o /etc/apt/trusted.gpg.d/vaulthalla.gpg

    echo "deb [arch=amd64] https://apt.vaulthalla.sh stable main" | \
      sudo tee /etc/apt/sources.list.d/vaulthalla.list > /dev/null

    sudo apt update
    sudo apt install vaulthalla

#### Debian Install Prompts

During installation, you will be prompted for the following:

**1. Initialize PostgreSQL database?**  
By default, the installer will attempt to create the required PostgreSQL
database and user automatically.

- Select **Yes** (recommended) to allow Vaulthalla to initialize PostgreSQL.
- Select **No** only if you intend to provision the database manually.

Manual setup instructions are available in:  
`/usr/share/doc/vaulthalla/README.Debian`

**2. Super-admin Linux UID**  
Vaulthalla maps a privileged internal super-admin account to a Linux user
for filesystem and administrative operations.

- Leave blank or select **auto** to assign the installing user automatically.
- Enter a numeric UID to bind super-admin privileges to a specific user.

This UID is assigned once and is not modified automatically thereafter.

---

### Option 2: Build from Source (Development Preview)

> ⚠️ **For development and testing only**

    git clone https://github.com/vaulthalla/server.git
    cd server
    sudo make install -- -d

### About `-d` (Developer Mode)

The `-d` flag enables **volatile development mode** intended **only** for testing:

- Debug builds
- Auto-provisioned admin user (`vh!adm1n`)
- Verbose logging
- Dev vaults and S3 test backends
- **Automatic database initialization and resets**

**⚠️ WARNING:**  
Developer mode **can and will wipe or reinitialize** the PostgreSQL database
and overwrite existing Vaulthalla configuration.

**Never use `-d` on a system containing real data.**  
Run it only inside VMs, containers, or disposable test environments.

---

## ✅ Verifying Services

    systemctl status vaulthalla-core vaulthalla-fuse
    journalctl -f -u vaulthalla-core

---

## ⚙️ System Notes

- HTTPS requires port **443**
- Default config: `/etc/vaulthalla/config.yaml`
- PostgreSQL is **managed internally** — do not modify schemas manually
- Back up **both** encryption keys *and* database state

---

## 💡 Support & Contribution

Issues, feedback, and battle scars welcome.  
Contributor intake coming soon.

---

## 🚧 Development Status

Public preview.  
Architecture docs and operator guides inbound.

---

### Mission Statement

**Vaulthalla is for people who refuse to rent back their own data.**

No subscriptions.  
No surveillance.  
No bloated stacks.

Just a battle-hardened system daemon; encrypted like state secrets, mounted directly into your filesystem, and fast enough to remind you why compiled software still matters.
