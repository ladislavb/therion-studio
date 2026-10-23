#!/usr/bin/env bash
set -euo pipefail

TMP_OUT="$(mktemp)"
TMP_CURRENT_NORM="$(mktemp)"
TMP_GENERATED_NORM="$(mktemp)"
TMP_COMMANDS_DIR="$(mktemp -d)"
TMP_REPORT="$(mktemp)"
cleanup() {
  rm -f "$TMP_OUT" "$TMP_CURRENT_NORM" "$TMP_GENERATED_NORM" "$TMP_REPORT"
  rm -rf "$TMP_COMMANDS_DIR"
}
trap cleanup EXIT

SOURCE_REF="snapshot"
SOURCE_REPO="https://github.com/therion/therion"
if [[ -f docs/therion-source.lock ]]; then
  SOURCE_REPO="$(python3 - <<'PY'
import json
from pathlib import Path
data = json.loads(Path("docs/therion-source.lock").read_text())
print(data.get("source_repository", "https://github.com/therion/therion"))
PY
)"
  SOURCE_REF="$(python3 - <<'PY'
import json
from pathlib import Path
data = json.loads(Path("docs/therion-source.lock").read_text())
print(data.get("source_reference", "snapshot"))
PY
)"
fi

python3 scripts/generate_therion_catalog.py \
  --output "$TMP_OUT" \
  --commands-dir "$TMP_COMMANDS_DIR" \
  --source-repo "$SOURCE_REPO" \
  --source-ref "$SOURCE_REF" \
  --report "$TMP_REPORT"

python3 - "$TMP_OUT" resources/therion_command_catalog.json "$TMP_GENERATED_NORM" "$TMP_CURRENT_NORM" <<'PY'
import json
import sys
from pathlib import Path

generated_path = Path(sys.argv[1])
current_path = Path(sys.argv[2])
generated_norm_path = Path(sys.argv[3])
current_norm_path = Path(sys.argv[4])

def normalize(path: Path) -> str:
    data = json.loads(path.read_text(encoding="utf-8"))
    metadata = data.get("metadata", {})
    if isinstance(metadata, dict):
        metadata.pop("generated_at_utc", None)
    return json.dumps(data, indent=2, ensure_ascii=False, sort_keys=True) + "\n"

generated_norm_path.write_text(normalize(generated_path), encoding="utf-8")
current_norm_path.write_text(normalize(current_path), encoding="utf-8")
PY

if ! diff -u "$TMP_CURRENT_NORM" "$TMP_GENERATED_NORM"; then
  echo "Therion command catalog is out of date. Run scripts/refresh_therion_catalog.sh --skip-update" >&2
  exit 1
fi

if ! diff -ru "resources/therion_catalog/commands" "$TMP_COMMANDS_DIR"; then
  echo "Per-command Therion catalog files are out of date. Run scripts/refresh_therion_catalog.sh --skip-update" >&2
  exit 1
fi

if ! diff -u docs/THERION_BOOK_DIFFERENCES.md "$TMP_REPORT"; then
  echo "docs/THERION_BOOK_DIFFERENCES.md is out of date. Run scripts/refresh_therion_catalog.sh --skip-update" >&2
  exit 1
fi

echo "Therion command catalog is up to date."
