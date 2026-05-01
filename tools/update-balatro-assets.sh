#!/usr/bin/env bash
# tools/update-balatro-assets.sh
# Sync Balatro's textures / fonts / sounds / gamecontrollerdb.txt from
# the user's local Steam install into ./assets/balatro/.
#
# Usage:
#   tools/update-balatro-assets.sh [--dry-run] [--with-2x] [--with-4x]
#
# Override the install location with the BALATRO_PATH env var.
# See docs/src/architecture/asset-pipeline.md for the full design.

set -euo pipefail

# -- args --------------------------------------------------------------
DRY_RUN=0
WITH_2X=0
WITH_4X=0
for arg in "$@"; do
  case "$arg" in
    --dry-run)  DRY_RUN=1 ;;
    --with-2x)  WITH_2X=1 ;;
    --with-4x)  WITH_4X=1 ;;
    -h|--help)
      sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "unknown arg: $arg" >&2
      exit 2
      ;;
  esac
done

# -- repo root ---------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST_ROOT="$REPO_ROOT/assets/balatro"

# -- locate Balatro ----------------------------------------------------
detect_balatro_path() {
  if [[ -n "${BALATRO_PATH:-}" ]]; then
    echo "$BALATRO_PATH"
    return
  fi
  local uname_s
  uname_s="$(uname -s)"
  case "$uname_s" in
    MINGW*|MSYS*|CYGWIN*)
      # MSYS2 sees C: as /c
      echo "/c/Program Files (x86)/Steam/steamapps/common/Balatro/Balatro.exe"
      ;;
    Darwin)
      echo "$HOME/Library/Application Support/Steam/steamapps/common/Balatro/Balatro.app/Contents/Resources/Balatro.love"
      ;;
    Linux)
      echo "$HOME/.steam/steam/steamapps/common/Balatro/Balatro.exe"
      ;;
    *)
      echo ""
      ;;
  esac
}

BALATRO_SRC="$(detect_balatro_path)"
if [[ -z "$BALATRO_SRC" || ! -f "$BALATRO_SRC" ]]; then
  cat >&2 <<EOF
error: cannot find Balatro install.
  tried: $BALATRO_SRC
  set BALATRO_PATH=/path/to/Balatro.exe (or .love) and re-run.
EOF
  exit 1
fi

echo "found Balatro at: $BALATRO_SRC"

# -- pick unzip --------------------------------------------------------
pick_unzipper() {
  if command -v unzip >/dev/null 2>&1; then
    echo "unzip"
  elif command -v 7z >/dev/null 2>&1; then
    echo "7z"
  else
    echo ""
  fi
}
UNZIPPER="$(pick_unzipper)"
if [[ -z "$UNZIPPER" ]]; then
  echo "error: need 'unzip' or '7z' in PATH to extract Balatro.exe" >&2
  exit 1
fi

# -- extract -----------------------------------------------------------
WORK_DIR="$(mktemp -d -t balatro-extract-XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "extracting to: $WORK_DIR"
# unzip exits 1 on warnings (e.g. backslash path seps from PowerShell-made
# zips); only treat exit >= 2 as a real failure.
case "$UNZIPPER" in
  unzip)
    rc=0
    unzip -q "$BALATRO_SRC" -d "$WORK_DIR" || rc=$?
    if [[ $rc -ge 2 ]]; then
      echo "error: unzip failed (exit $rc)" >&2
      exit "$rc"
    fi
    ;;
  7z) 7z x -bso0 -bsp0 -o"$WORK_DIR" "$BALATRO_SRC" >/dev/null ;;
esac

RES_DIR="$WORK_DIR/resources"
if [[ ! -d "$RES_DIR" ]]; then
  echo "error: extracted archive has no resources/ dir; wrong file?" >&2
  exit 1
fi

# -- version sanity check ---------------------------------------------
# Balatro stores its version in version.jkr at the archive root.
VERSION_FILE="$WORK_DIR/version.jkr"
if [[ -f "$VERSION_FILE" ]]; then
  VERSION="$(tr -d '\r\n' < "$VERSION_FILE")"
  echo "Balatro version: $VERSION"
else
  echo "warn: no version.jkr found (skipping version sanity check)"
fi

# -- copy plan ---------------------------------------------------------
# Each entry: "<src-rel-to-resources>|<dest-rel-to-DEST_ROOT>"
PLAN=()
PLAN+=("textures/1x|textures/1x")
[[ $WITH_2X -eq 1 ]] && PLAN+=("textures/2x|textures/2x")
[[ $WITH_4X -eq 1 ]] && PLAN+=("textures/4x|textures/4x")
PLAN+=("fonts|fonts")
PLAN+=("sounds|sounds")

copy_tree() {
  local src="$1"
  local dst="$2"
  if [[ ! -d "$src" ]]; then
    echo "  skip (missing in source): $src"
    return
  fi
  if [[ $DRY_RUN -eq 1 ]]; then
    # list files only, count bytes
    local count bytes
    count=$(find "$src" -type f | wc -l)
    bytes=$(find "$src" -type f -printf '%s\n' 2>/dev/null | awk '{s+=$1} END {print s+0}')
    printf "  [dry] %-22s %4d files  %10d bytes -> %s\n" \
      "$(basename "$src")" "$count" "$bytes" "$dst"
    TOTAL_BYTES=$((TOTAL_BYTES + bytes))
    TOTAL_FILES=$((TOTAL_FILES + count))
  else
    mkdir -p "$dst"
    cp -R "$src/." "$dst/"
    echo "  copied: $src -> $dst"
  fi
}

TOTAL_BYTES=0
TOTAL_FILES=0

echo "destination: $DEST_ROOT"
[[ $DRY_RUN -eq 0 ]] && mkdir -p "$DEST_ROOT"

for entry in "${PLAN[@]}"; do
  src_rel="${entry%%|*}"
  dst_rel="${entry##*|}"
  copy_tree "$RES_DIR/$src_rel" "$DEST_ROOT/$dst_rel"
done

# Single file: gamecontrollerdb.txt
GCDB_SRC="$RES_DIR/gamecontrollerdb.txt"
GCDB_DST="$DEST_ROOT/gamecontrollerdb.txt"
if [[ -f "$GCDB_SRC" ]]; then
  if [[ $DRY_RUN -eq 1 ]]; then
    bytes=$(stat -c '%s' "$GCDB_SRC" 2>/dev/null || stat -f '%z' "$GCDB_SRC")
    printf "  [dry] %-22s %4d files  %10d bytes -> %s\n" \
      "gamecontrollerdb.txt" 1 "$bytes" "$GCDB_DST"
    TOTAL_BYTES=$((TOTAL_BYTES + bytes))
    TOTAL_FILES=$((TOTAL_FILES + 1))
  else
    cp "$GCDB_SRC" "$GCDB_DST"
    echo "  copied: gamecontrollerdb.txt"
  fi
fi

# -- summary -----------------------------------------------------------
if [[ $DRY_RUN -eq 1 ]]; then
  printf "\n[dry-run] would copy %d files, %d bytes (~%d MiB)\n" \
    "$TOTAL_FILES" "$TOTAL_BYTES" "$((TOTAL_BYTES / 1024 / 1024))"
  echo "[dry-run] no files were modified."
else
  echo "done. assets are under: $DEST_ROOT"
  echo "reminder: shaders/ is NOT auto-synced — port by hand to GLSL 330."
  echo "         see docs/src/architecture/shader-uniforms.md"
fi
