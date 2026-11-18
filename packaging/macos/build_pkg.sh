#!/usr/bin/env bash
# Builds the AuroraTrigger macOS installer (.pkg) with AU and VST3 as selectable components.
#
# Usage: packaging/macos/build_pkg.sh <artefacts/Release dir> <version> <output.pkg>
#   e.g. packaging/macos/build_pkg.sh build/AuroraTrigger_artefacts/Release 0.1.0 dist/AuroraTrigger.pkg
#
# Optional: PKG_ARCHS=arm64 | x86_64 restricts the installer to that Mac architecture (default: both).
# Optional: set INSTALLER_SIGN_IDENTITY="Developer ID Installer: ..." to sign the .pkg.

set -euo pipefail

ART="$1"
VERSION="$2"
OUT="$3"

HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

ID_PREFIX="com.auroraaudio.auroratrigger"

build_component () {
    local name="$1" bundle="$2" location="$3"
    local root="$WORK/root-$name"
    local plist="$WORK/$name.plist"

    if [ ! -d "$bundle" ]; then
        echo "error: $bundle not found" >&2
        exit 1
    fi

    mkdir -p "$root"
    ditto "$bundle" "$root/$(basename "$bundle")"

    # Install exactly where we say, never "relocate" to another copy of the bundle found on disk.
    pkgbuild --analyze --root "$root" "$plist"
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" > /dev/null 2>&1; do
        # plutil -replace also creates the key when pkgbuild left it out
        plutil -replace "$i.BundleIsRelocatable" -bool NO "$plist"
        i=$((i + 1))
    done

    local args=(--root "$root"
                --component-plist "$plist"
                --identifier "$ID_PREFIX.$name"
                --version "$VERSION"
                --install-location "$location")

    if [ -d "$HERE/scripts-$name" ]; then
        args+=(--scripts "$HERE/scripts-$name")
    fi

    pkgbuild "${args[@]}" "$WORK/AuroraTrigger-$name.pkg"
}

build_component au   "$ART/AU/Aurora Trigger.component" "/Library/Audio/Plug-Ins/Components"
build_component vst3 "$ART/VST3/Aurora Trigger.vst3" "/Library/Audio/Plug-Ins/VST3"

sed -e "s/@VERSION@/$VERSION/g" -e "s/@ARCHS@/${PKG_ARCHS:-arm64,x86_64}/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"

mkdir -p "$(dirname "$OUT")"

sign_args=()
if [ -n "${INSTALLER_SIGN_IDENTITY:-}" ]; then
    sign_args=(--sign "$INSTALLER_SIGN_IDENTITY")
fi

productbuild --distribution "$WORK/distribution.xml" \
             --package-path "$WORK" \
             --resources "$HERE/resources" \
             "${sign_args[@]+"${sign_args[@]}"}" \
             "$OUT"

echo "Installer written to $OUT"
