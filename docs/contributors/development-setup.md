# Development Setup

This guide is for contributors getting from clone to useful local work.

If you are doing docs-only work, you do not need the full Linux runtime environment. If you are touching core runtime, FUSE, packaging, systemd, PostgreSQL, TPM/swtpm, or install lifecycle behavior, you do.

## Platform Assumptions

Vaulthalla is Linux-native software.

Meaningful validation for these areas requires a Linux environment:

- core daemon work
- FUSE behavior
- package lifecycle scripts
- systemd services
- PostgreSQL bootstrap and state handling
- TPM or `swtpm` integration

Debian and Ubuntu-style environments are the safest contributor path because the repo scripts and package metadata assume `apt`, `sudo`, systemd, and Debian packaging.

If you are on macOS or Windows, docs work is fine, and some web or release-tooling work may still be possible. Do not pretend that means you fully validated Linux runtime behavior.

## Quick Health Check

From the repo root:

```bash
bash .codex/scripts/doctor.sh
bash .codex/scripts/index.sh
```

`doctor.sh` checks for `git`, `python3`, `node`, `pnpm`, `meson`, `ninja`, the presence of `web/node_modules`, and the top-level project files the repo expects.

## Toolchains

### Web/admin work

The web client uses:

- Node pinned by `web/.nvmrc` (`24.13`)
- `pnpm`
- Next.js, React, TypeScript, ESLint

Setup:

```bash
cd web
pnpm install --frozen-lockfile
pnpm test
pnpm dev
```

### Release-tooling work

The release tooling uses Python dependencies from `requirements.txt`.

Setup:

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python3 -m tools.release check
```

### Core/runtime work

For Linux contributors on a machine or VM you are comfortable modifying, the repo provides an install-deps helper:

```bash
./bin/setup/install_deps.sh
```

That script adds the Vaulthalla APT repository if needed and installs native dependencies such as Meson, Ninja, PostgreSQL, FUSE, PDFium, `libpqxx`, `libsodium`, `libtss2`, `swtpm`, and related packages.

Run it deliberately. It changes the host.

## Core Build Path

The current CI-style core build path is Meson-based:

```bash
meson setup build core --buildtype=debug -Dbuild_unit_tests=true -Dinstall_data=false
meson compile -C build
meson test -C build
```

That matches the shape used in `.github/actions/build/action.yml` and `.github/actions/test/action.yml`.

If your work needs an installed runtime rather than just a compiled binary, the repository also provides higher-level install and test helpers under `bin/` and `core/bin/`.

## Integration And Runtime Test Path

For Linux-only runtime validation:

```bash
./bin/tests/install.sh --run
./bin/tests/uninstall.sh
```

What that setup path does:

- installs native dependencies
- prepares users and directories
- prepares PostgreSQL state
- builds the core integration target
- runs the integration binary when `--run` is provided

This path depends on `sudo`, Linux, FUSE, PostgreSQL, and other host-level pieces. Use it when your change affects runtime behavior rather than simple docs or web copy.

## Local Config And Environment Notes

- Default runtime config ships from `deploy/config/config.yaml`.
- Install scripts copy config into `/etc/vaulthalla/config.yaml`.
- The web service unit uses `deploy/systemd/vaulthalla-web.service.in`.
- Some local scripts look for `deploy/vaulthalla.env`.

The repo only ships `deploy/vaulthalla.env.example` as a public template. If you need real storage-backed test credentials or environment values, create your own local `deploy/vaulthalla.env` from that template and keep secrets out of commits.

## Known Practical Snags

### Missing `web/node_modules`

`bash .codex/scripts/doctor.sh` will flag this. Run:

```bash
cd web
pnpm install --frozen-lockfile
```

### Web build failures caused by missing private icons

The CI web build syncs private icons from `~/vaulthalla-web-icons` before `pnpm build`. If local web build work depends on those assets, coordinate with the maintainer instead of committing fake replacements.

### Integration test scripts expecting `deploy/vaulthalla.env`

Some scripts reference `deploy/vaulthalla.env`, while the repo only includes `deploy/vaulthalla.env.example`. If your test path needs environment values, create the local file explicitly.

### Cross-platform false confidence

If you changed FUSE, packaging, auth/session, systemd, PostgreSQL lifecycle, or TPM behavior from a non-Linux machine, your local check is not enough. Say that plainly in the PR and ask for the right Linux validation.

## Where To Go Next

- Read [Contribution Boundaries](./contribution-boundaries.md) before picking sharp work.
- Use the [Architecture Map](./architecture-map.md) to find the right subsystem.
- Follow the [Validation Guide](./validation-guide.md) before opening a PR.
