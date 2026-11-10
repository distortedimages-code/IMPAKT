#!/bin/bash
# IMPAKT for Linux: an archive with the VST3 plug-in, the standalone app, CHIMERA's sounds (one copy for both) and
# install.sh / uninstall.sh (for the user: ~/.vst3, ~/.local/share/IMPAKT, a menu entry; no root needed).
#   packaging/linux/make_tar.sh <IMPAKT_artefacts/Release> <output folder>   ->  <output folder>/IMPAKT-Linux.tar.gz
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
ART=$(cd "$1" && pwd)
mkdir -p "$2"; OUT=$(cd "$2" && pwd)
VER=$(head -n 1 "$ROOT/VERSION" | tr -d '[:space:]')
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
D="$WORK/IMPAKT-$VER-Linux"
mkdir -p "$D/app" "$D/bank"
cp -a "$ART/VST3/IMPAKT.vst3" "$D/IMPAKT.vst3"
cp -a "$ART/Standalone/IMPAKT" "$D/app/IMPAKT"
cp "$ROOT"/bank/chimera.* "$D/bank/"
[ -f "$D/bank/chimera.idx" ] || { echo "missing CHIMERA's bank (bank/)"; exit 1; }
[ -d "$D/IMPAKT.vst3/Contents/Resources/bank" ] && { echo "the VST3 holds its own bank: build with -DIMPAKT_EMBED_BANK=OFF"; exit 1; }
strip --strip-unneeded "$D/app/IMPAKT" "$D/IMPAKT.vst3/Contents/x86_64-linux/IMPAKT.so" || true
cp "$HERE/install.sh" "$HERE/uninstall.sh" "$HERE/impakt.desktop" "$HERE/LISEZ-MOI.txt" "$D/"
cp "$ROOT/LICENSE" "$D/LICENSE.txt"
cp "$ROOT/brand/impakt-icon-256.png" "$D/impakt.png"
chmod +x "$D/install.sh" "$D/uninstall.sh" "$D/app/IMPAKT"
tar -C "$WORK" -czf "$OUT/IMPAKT-Linux.tar.gz" "IMPAKT-$VER-Linux"
ls -l "$OUT/IMPAKT-Linux.tar.gz"
