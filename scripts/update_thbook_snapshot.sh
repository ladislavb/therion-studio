#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/update_thbook_snapshot.sh --repo <therion-repo-path-or-url> --ref <tag-or-commit>

Example:
  scripts/update_thbook_snapshot.sh --repo https://github.com/therion/therion.git --ref v5.5.16
EOF
}

REPO=""
REF=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      REPO="${2:-}"
      shift 2
      ;;
    --ref)
      REF="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$REPO" || -z "$REF" ]]; then
  echo "Both --repo and --ref are required." >&2
  usage
  exit 1
fi

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

sha256_file() {
  local file_path="$1"
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file_path" | awk '{print $1}'
    return 0
  fi
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file_path" | awk '{print $1}'
    return 0
  fi
  python3 - "$file_path" <<'PY'
import hashlib
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
print(hashlib.sha256(path.read_bytes()).hexdigest())
PY
}

if [[ -d "$REPO/.git" ]]; then
  REPO_DIR="$REPO"
else
  REPO_DIR="$TMP_DIR/upstream"
  git clone --quiet --no-checkout "$REPO" "$REPO_DIR"
fi

mkdir -p "$TMP_DIR/extract"
git -C "$REPO_DIR" archive "$REF" \
  thbook/ch02.tex \
  thbook/ch03.tex \
  thbook/thbook.tex \
  src/therion-core \
  | tar -x -C "$TMP_DIR/extract"

mkdir -p therion/thbook
cp "$TMP_DIR/extract/thbook/ch02.tex" therion/thbook/ch02.tex
cp "$TMP_DIR/extract/thbook/ch03.tex" therion/thbook/ch03.tex
cp "$TMP_DIR/extract/thbook/thbook.tex" therion/thbook/thbook.tex
# Option tables and parsing code that the catalog generator reads.
rm -rf therion/src/therion-core
mkdir -p therion/src
cp -R "$TMP_DIR/extract/src/therion-core" therion/src/therion-core

CH02_SHA="$(sha256_file therion/thbook/ch02.tex)"
CH03_SHA="$(sha256_file therion/thbook/ch03.tex)"
THBOOK_SHA="$(sha256_file therion/thbook/thbook.tex)"
# One hash over the generator's Therion source inputs (sorted .h and .cxx files).
CORE_SHA="$(find therion/src/therion-core -maxdepth 1 \( -name '*.h' -o -name '*.cxx' \) | LC_ALL=C sort | xargs cat | sha256_file /dev/stdin)"
UPDATED_AT="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

cat > docs/therion-source.lock <<EOF
{
  "source_repository": "$REPO",
  "source_reference": "$REF",
  "updated_at_utc": "$UPDATED_AT",
  "files": [
    { "path": "therion/thbook/ch02.tex", "sha256": "$CH02_SHA" },
    { "path": "therion/thbook/ch03.tex", "sha256": "$CH03_SHA" },
    { "path": "therion/thbook/thbook.tex", "sha256": "$THBOOK_SHA" },
    { "path": "therion/src/therion-core/*.h *.cxx", "sha256": "$CORE_SHA" }
  ]
}
EOF

echo "Updated thbook snapshot and docs/therion-source.lock from $REPO@$REF"
