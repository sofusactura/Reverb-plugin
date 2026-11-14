#!/bin/bash
# Builds the macOS installer package (VST3 + Audio Unit), installed for all users.
# Usage: installer/macos.sh <version> <path to artefacts/Release> <output folder>
set -euo pipefail

VERSION="$1"
RELEASE="$2"
OUT="$3"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

# One component package per plugin format.
make_component() {   # <name> <bundle path> <install location>
    local name="$1" bundle="$2" location="$3"
    mkdir -p "$WORK/$name"
    cp -R "$bundle" "$WORK/$name/"

    # Stop the installer from "relocating" the plugin to wherever another copy happens to be.
    pkgbuild --analyze --root "$WORK/$name" "$WORK/$name.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$name.plist"

    pkgbuild --root "$WORK/$name" --component-plist "$WORK/$name.plist" \
             --identifier "dk.actura.hybridgatereverb.$name" --version "$VERSION" \
             --install-location "$location" "$WORK/$name.pkg"
}

make_component vst3 "$RELEASE/VST3/HybridGate Reverb.vst3" "/Library/Audio/Plug-Ins/VST3"
make_component au "$RELEASE/AU/HybridGate Reverb.component" "/Library/Audio/Plug-Ins/Components"

productbuild --package "$WORK/vst3.pkg" --package "$WORK/au.pkg" \
             --identifier "dk.actura.hybridgatereverb" --version "$VERSION" \
             "$OUT/HybridGateReverb-$VERSION-macOS.pkg"

echo "Built $OUT/HybridGateReverb-$VERSION-macOS.pkg"
