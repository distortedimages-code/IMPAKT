#!/bin/bash
# IMPAKT's Mac disk image: an installer (VST3, AU and the app, each one a choice) and, beside it, the three bundles with
# shortcuts to their folders for those who drag them by hand.
#   packaging/mac/make_dmg.sh <IMPAKT_artefacts/Release> <output folder>   ->  <output folder>/IMPAKT-macOS.dmg
# Needs: macOS (pkgbuild, productbuild, hdiutil, tiffutil, codesign) and dmgbuild (pip install dmgbuild).
# The bundles are signed ad hoc: without an Apple developer certificate macOS asks once to allow the installer (the
# site's install guide shows where). With one, sign them with it here and notarise the disk image.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
ART=$(cd "$1" && pwd)
mkdir -p "$2"; OUT=$(cd "$2" && pwd)
VER=$(head -n 1 "$ROOT/VERSION" | tr -d '[:space:]')
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
echo "IMPAKT $VER: disk image from $ART"

mkdir -p "$WORK/root/vst3" "$WORK/root/au" "$WORK/root/app" "$WORK/pkgs" "$WORK/stage"
ditto "$ART/VST3/IMPAKT.vst3"       "$WORK/root/vst3/IMPAKT.vst3"
ditto "$ART/AU/IMPAKT.component"    "$WORK/root/au/IMPAKT.component"
ditto "$ART/Standalone/IMPAKT.app"  "$WORK/root/app/IMPAKT.app"
for b in "$WORK/root/vst3/IMPAKT.vst3" "$WORK/root/au/IMPAKT.component" "$WORK/root/app/IMPAKT.app"; do
    [ -f "$b/Contents/Resources/bank/chimera.idx" ] || { echo "missing CHIMERA's bank in $b"; exit 1; }
    lipo -archs "$b/Contents/MacOS/"* | tee /dev/stderr | grep -q "x86_64 arm64\|arm64 x86_64" || echo "warning: $b is not universal"
    codesign --force --deep --sign - "$b"
done

# one package per bundle, none relocatable (an older copy elsewhere on the disk must not catch the install)
pkg () {   # $1 root, $2 id, $3 install location, [$4 scripts]
    pkgbuild --analyze --root "$WORK/root/$1" "$WORK/$1.plist" >/dev/null
    /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$WORK/$1.plist"
    pkgbuild --root "$WORK/root/$1" --component-plist "$WORK/$1.plist" --identifier "com.impaktaudio.impakt.$1" \
        --version "$VER" --install-location "$3" ${4:+--scripts "$4"} "$WORK/pkgs/$1.pkg"
}
pkg vst3 com.impaktaudio.impakt.vst3 "/Library/Audio/Plug-Ins/VST3"       "$HERE/scripts-plugin"
pkg au   com.impaktaudio.impakt.au   "/Library/Audio/Plug-Ins/Components" "$HERE/scripts-au"
pkg app  com.impaktaudio.impakt.app  "/Applications"

sed "s/@VERSION@/$VER/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/res"; sed "s/@VERSION@/$VER/g" "$HERE/resources/welcome.html" > "$WORK/res/welcome.html"
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK/pkgs" "$WORK/stage/Install IMPAKT.pkg"

# the disk image: the installer, the bundles (the same ones), the read-me; the shortcuts and the window (dmg_settings.py)
ditto "$WORK/root/vst3/IMPAKT.vst3"      "$WORK/stage/IMPAKT.vst3"
ditto "$WORK/root/au/IMPAKT.component"   "$WORK/stage/IMPAKT.component"
ditto "$WORK/root/app/IMPAKT.app"        "$WORK/stage/IMPAKT.app"
cp "$HERE/LISEZ-MOI.txt" "$WORK/stage/LISEZ-MOI.txt"
tiffutil -cathidpicheck "$HERE/dmg-background.png" "$HERE/dmg-background@2x.png" -out "$WORK/background.tiff"
rm -f "$OUT/IMPAKT-macOS.dmg"
dmgbuild -s "$HERE/dmg_settings.py" -D stage="$WORK/stage" -D bg="$WORK/background.tiff" "IMPAKT $VER" "$OUT/IMPAKT-macOS.dmg"
hdiutil verify "$OUT/IMPAKT-macOS.dmg"
ls -l "$OUT/IMPAKT-macOS.dmg"
