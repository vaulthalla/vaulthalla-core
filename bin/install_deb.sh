#!/usr/bin/env bash
set -euo pipefail

# =========================[ CONFIG ]=========================
: "${NEXUS_HOST:=https://repo.valkyrianlabs.com}"
: "${NEXUS_VAULTHALLA_REPO:=vaulthalla-apt}"
: "${NEXUS_VALKYRIAN_REPO:=valkyrian-apt}"
: "${UPLOAD_USER:=cooper}"
: "${UPLOAD_PASS:=${NEXUS_COOPER_PASSWORD:-}}"

: "${DEBIAN_BRANCH:=debian}"

# Toggle: push artifacts when --push is used
PUSH=0

# ======================[ UTIL / PATHS ]======================
log() { printf "\033[1;36m[%s]\033[0m %s\n" "$(date +'%H:%M:%S')" "$*"; }
die() { printf "\033[1;31mERROR:\033[0m %s\n" "$*" >&2; exit 1; }
require() { command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# =======================[ ARG PARSE ]========================
usage() {
  cat <<'EOF'
Usage: bin/release-deb.sh [--push] [--sign] [--no-dch]

  --push     Upload resulting .deb/.dsc/.changes to Nexus APT repos
  --sign     Build signed source/changes (omit -us -uc)
  --no-dch   Skip 'gbp dch' (use existing debian/changelog as-is)

Environment:
  NEXUS_VAULTHALLA_HOST, NEXUS_VALKYRIAN_HOST
  NEXUS_VAULTHALLA_REPO, NEXUS_VALKYRIAN_REPO
  UPLOAD_USER, UPLOAD_PASS (or NEXUS_COOPER_PASSWORD)
  DEBIAN_BRANCH (default: debian)
EOF
}

SIGN=0
RUN_DCH=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --push) PUSH=1; shift ;;
    --sign) SIGN=1; shift ;;
    --no-dch) RUN_DCH=0; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown arg: $1" ;;
  esac
done

# =====================[ PRE-FLIGHT ]=========================
require git
require gbp
require dpkg-buildpackage
require dpkg-parsechangelog
require sed
require awk
require pandoc
[[ $PUSH -eq 1 ]] && require curl

# Ensure we’re in a git repo and on correct branch
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "Not in a git repo"
CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ -n "$CURRENT_BRANCH" ]] || die "Cannot determine current branch"

# Get package name from debian/control (Source:)
PKG_NAME="$(awk '/^Source:/ {print tolower($2); exit}' debian/control)"
[[ -n "$PKG_NAME" ]] || die "Failed to parse Source: from debian/control"

# Extract upstream version from meson.build (first version: 'x.y.z')
MESON_VERSION="$(grep -Po "(?<=version:\s*')[^']+" meson.build | head -1 || true)"
[[ -n "$MESON_VERSION" ]] || die "Could not parse version from meson.build"

# Use DEBEMAIL/DEBFULLNAME or fall back to git config for dch
export DEBEMAIL="${DEBEMAIL:-$(git config --get user.email || true)}"
export DEBFULLNAME="${DEBFULLNAME:-$(git config --get user.name || true)}"
[[ -n "$DEBEMAIL" && -n "$DEBFULLNAME" ]] || die "Set DEBEMAIL/DEBFULLNAME or configure git user.name/user.email"

# =====================[ MANPAGE ]=====================
pandoc -s -t man deploy/vh.md -o debian/vh.1

# ===================[ UPDATE CHANGELOG ]=====================
if [[ $RUN_DCH -eq 1 ]]; then
  log "Generating debian/changelog with gbp dch…"
  # New debian version as upstream + '-1' unless already present
  gbp dch \
    --debian-branch="$DEBIAN_BRANCH" \
    --auto \
    --commit \
    --new-version="${MESON_VERSION}-1" \
    --verbose
else
  log "Skipping gbp dch (using existing changelog)"
fi

# Read the resulting Debian version for artifacts (includes -1)
DEB_VERSION="$(dpkg-parsechangelog --show-field Version)"
[[ -n "$DEB_VERSION" ]] || die "dpkg-parsechangelog did not yield a Version"

# Build artifact dir under bin/ (scoped to /bin, not ../)
ARTIFACT_ROOT="$REPO_ROOT/bin/artifacts/$DEB_VERSION"
mkdir -p "$ARTIFACT_ROOT"

# =====================[ ISOLATED BUILD ]=====================
TMP_BUILD_ROOT="$(mktemp -d)"
cleanup() { rm -rf "$TMP_BUILD_ROOT"; }
trap cleanup EXIT

log "Creating isolated build tree…"
# Preserve .git so dpkg-parsechangelog etc still behave if needed
rsync -a --exclude 'bin/artifacts' "$REPO_ROOT/" "$TMP_BUILD_ROOT/src/"

cd "$TMP_BUILD_ROOT/src"

log "Building packages…"
if [[ $SIGN -eq 1 ]]; then
  dpkg-buildpackage -b
else
  dpkg-buildpackage -us -uc -b
fi

log "Collecting artifacts → $ARTIFACT_ROOT"
cd "$TMP_BUILD_ROOT"
shopt -s nullglob
for f in *.deb *.dsc *.changes *.buildinfo *.orig.tar.* *.debian.tar.*; do
  cp -v "$f" "$ARTIFACT_ROOT/"
done
shopt -u nullglob

# ======================[ OPTIONAL PUSH ]=====================
if [[ $PUSH -eq 1 ]]; then
  [[ -n "$UPLOAD_PASS" ]] || die "UPLOAD_PASS is empty (set UPLOAD_PASS or NEXUS_COOPER_PASSWORD)"

  # Compute pool path: pool/main/<first>/<pkg>/
  first="$(printf "%s" "$PKG_NAME" | cut -c1)"
  VAULTHALLA_BASE="${NEXUS_HOST%/}/repository/${NEXUS_VAULTHALLA_REPO}"
  VALKYRIAN_BASE="${NEXUS_HOST%/}/repository/${NEXUS_VALKYRIAN_REPO}"

  log "Uploading to Nexus APT (Vaulthalla): $VAULTHALLA_BASE"
  for f in "$ARTIFACT_ROOT"/*; do
    fname="$(basename "$f")"
    log "  → $fname"
    curl -sSf -u "$UPLOAD_USER:$UPLOAD_PASS" --upload-file "$f" "${VAULTHALLA_BASE}/${fname}"
  done

  log "Uploading to Nexus APT (Valkyrian): $VALKYRIAN_BASE"
  for f in "$ARTIFACT_ROOT"/*; do
    fname="$(basename "$f")"
    log "  → $fname"
    curl -sSf -u "$UPLOAD_USER:$UPLOAD_PASS" --upload-file "$f" "${VALKYRIAN_BASE}/${fname}"
  done

  log "Uploads complete. Nexus APT will handle indexing/signing per repo config."
else
  log "Skipping upload (no --push). Artifacts are in: $ARTIFACT_ROOT"
fi

log "✅ Done: $DEB_VERSION"
