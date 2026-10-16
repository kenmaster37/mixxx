#!/usr/bin/env bash
# Builds the Linux .deb from the Windows working tree, inside WSL.
#
#   wsl -d Ubuntu-24.04 -u root bash wsl_rebuild_deb.sh [output-name-base]
#
# The Linux build directory is reused (incremental), so this takes minutes
# instead of a full build. It never touches build/, buildenv/ or .git on the
# Windows side, and copies the resulting package to dist/.
set -euo pipefail

SRC="/mnt/c/Users/giova/Desktop/mixxx/mixxx"
DST="/home/mixxx/mixxx"
BUILD="$DST/build"
DIST="/mnt/c/Users/giova/Desktop/mixxx/dist"
SHIM="/home/mixxx/toolshim"
NAME="${1:-}"

if [ ! -d "$SRC" ]; then
    echo "no encuentro $SRC" >&2
    exit 1
fi

echo "== Sincronizando el arbol de fuentes (build/ de Linux se conserva) =="
rsync -a --delete \
    --exclude 'build/' \
    --exclude 'buildenv/' \
    --exclude '.git/' \
    --exclude 'dist/' \
    --exclude 'stemlab/' \
    "$SRC/" "$DST/"

echo "== Normalizando permisos (NTFS no tiene modos POSIX) =="
chown -R mixxx:mixxx "$DST"
find "$DST" -type d -exec chmod 755 {} +
find "$DST" -type f -exec chmod 644 {} +

echo "== Compilando como usuario mixxx =="
runuser -u mixxx -- env PATH="$SHIM:$PATH" cmake --build "$BUILD" -j"$(nproc)"

echo "== Empaquetando .deb =="
cd "$BUILD"
runuser -u mixxx -- env PATH="$SHIM:$PATH" cpack -G DEB

DEB="$(ls -t "$BUILD"/*.deb | head -1)"
echo "== Copiando $DEB a dist/ =="
mkdir -p "$DIST"
if [ -n "$NAME" ]; then
    cp "$DEB" "$DIST/$NAME"
    echo "listo: $DIST/$NAME"
else
    cp "$DEB" "$DIST/"
    echo "listo: $DIST/$(basename "$DEB")"
fi
