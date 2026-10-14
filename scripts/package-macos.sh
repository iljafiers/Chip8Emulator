#!/bin/sh
# Builds a release Chip8Emulator.app (universal: Intel + Apple Silicon),
# bundles the Qt frameworks into it, ad-hoc signs it and packs it into a DMG.
#
# Usage: scripts/package-macos.sh [path to Qt, default ~/Qt/6.12.0/macos]
#
# The app is NOT signed with an Apple Developer ID and NOT notarized, so
# Gatekeeper warns on first launch (see "Installing on macOS" in README.md).

set -e

QT_DIR="${1:-$HOME/Qt/6.12.0/macos}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^VERSION *= *//p' "$ROOT/Chip8Emulator.pro")"
BUILD="$ROOT/build/release"
APP="$BUILD/Chip8Emulator.app"
DMG="$ROOT/build/Chip8Emulator-$VERSION-macOS.dmg"

rm -rf "$BUILD" "$DMG"
mkdir -p "$BUILD"
cd "$BUILD"
"$QT_DIR/bin/qmake" "$ROOT/Chip8Emulator.pro" CONFIG+=release
make -j"$(sysctl -n hw.ncpu)"

# qmake's generated Info.plist has no version; Finder and "Get Info" show these
plutil -insert CFBundleShortVersionString -string "$VERSION" "$APP/Contents/Info.plist"
plutil -insert CFBundleVersion -string "$VERSION" "$APP/Contents/Info.plist"
plutil -insert CFBundleName -string "Chip8 Emulator" "$APP/Contents/Info.plist"
plutil -insert NSHumanReadableCopyright -string "Copyright (c) 2026 Ilja Fiers" "$APP/Contents/Info.plist"

# Copy the Qt frameworks and plugins into the bundle, then ad-hoc sign it
# ("-"): Apple Silicon refuses to run unsigned code, but an ad-hoc signature
# needs no developer account.
"$QT_DIR/bin/macdeployqt" "$APP" -codesign=-
codesign --verify --deep --strict "$APP"

# DMG with the app and a shortcut to /Applications to drag it onto
STAGE="$BUILD/dmg"
mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "Chip8 Emulator $VERSION" -srcfolder "$STAGE" \
    -format UDZO -ov "$DMG"
rm -rf "$STAGE"

echo "Created $DMG"
