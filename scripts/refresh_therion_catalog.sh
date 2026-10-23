#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/refresh_therion_catalog.sh [--repo <therion-repo-path-or-url> --ref <tag-or-commit>] [--skip-update]

Examples:
  scripts/refresh_therion_catalog.sh --repo https://github.com/therion/therion.git --ref v5.5.16
  scripts/refresh_therion_catalog.sh --skip-update
EOF
}

REPO=""
REF=""
SKIP_UPDATE=0

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
    --skip-update)
      SKIP_UPDATE=1
      shift
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

read_lock_value() {
  local key="$1"
  if [[ ! -f docs/therion-source.lock ]]; then
    return 0
  fi
  python3 - "$key" <<'PY'
import json
import sys
from pathlib import Path

key = sys.argv[1]
data = json.loads(Path("docs/therion-source.lock").read_text())
print(data.get(key, ""))
PY
}

if [[ -z "$REPO" ]]; then
  REPO="$(read_lock_value source_repository)"
fi
if [[ -z "$REF" ]]; then
  REF="$(read_lock_value source_reference)"
fi

if [[ "$SKIP_UPDATE" -eq 0 ]]; then
  if [[ -z "$REPO" || -z "$REF" ]]; then
    echo "When not using --skip-update, provide --repo/--ref or set them in docs/therion-source.lock." >&2
    usage
    exit 1
  fi
  scripts/update_thbook_snapshot.sh --repo "$REPO" --ref "$REF"
fi

SOURCE_REF="snapshot"
SOURCE_REPO="https://github.com/therion/therion"
LOCKED_REPO="$(read_lock_value source_repository)"
LOCKED_REF="$(read_lock_value source_reference)"
if [[ -n "$LOCKED_REPO" ]]; then
  SOURCE_REPO="$LOCKED_REPO"
fi
if [[ -n "$LOCKED_REF" ]]; then
  SOURCE_REF="$LOCKED_REF"
fi

python3 scripts/generate_therion_catalog.py \
  --source-repo "$SOURCE_REPO" \
  --source-ref "$SOURCE_REF" \
  --report docs/THERION_BOOK_DIFFERENCES.md

echo "Refreshed resources/therion_command_catalog.json"
echo "Refreshed resources/therion_catalog/commands/*.json"
