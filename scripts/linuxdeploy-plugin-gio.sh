#!/usr/bin/env bash
#
# Quick n' Dirty LinuxDeploy plugin that copies GIO modules, needed
# for the AppImage to correctly pick up dark mode preference under
# GNOME in some Ubuntu host versions
#
set -eu

APPDIR=

while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugin-api-version)
            echo "0"
            exit 0
            ;;
        --appdir)
            APPDIR="$2"
            shift
            shift
            ;;
        *)
            echo "Invalid argument $1"
            exit 1
            ;;
    esac
done

if [ "$APPDIR" == "" ]; then
    echo "--appdir must be provided"
    exit 1
fi

# Copy relevant module
GIO_MODULES_SRC_DIR=$(pkg-config --variable=giomoduledir gio-2.0)
GIO_MODULES_DEST_DIR="$APPDIR/usr/lib/gio/modules"
MODULE_PARAMS=()

mkdir -p "$GIO_MODULES_DEST_DIR"

for module in "libdconfsettings.so"; do
    if [ -f "$GIO_MODULES_SRC_DIR/$module" ]; then
        cp -v "$GIO_MODULES_SRC_DIR/$module" "$GIO_MODULES_DEST_DIR"
        patchelf --set-rpath '$ORIGIN:$ORIGIN/../..' "$GIO_MODULES_DEST_DIR/$module"
    fi
done

# Build GIO cache
gio-querymodules "$GIO_MODULES_DEST_DIR"

# Install hook
HOOKS_DIR="$APPDIR/apprun-hooks"

mkdir -p "$HOOKS_DIR"
cat > "$HOOKS_DIR/gio" <<\EOF
export APPDIR="${APPDIR:-"$(dirname "$(realpath "$0")")"}"
export GIO_MODULE_DIR="$APPDIR/usr/lib/gio/modules/"
EOF
