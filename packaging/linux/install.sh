#!/bin/sh
# IMPAKT for Linux: installs for you alone (no root): the VST3 in ~/.vst3, the app in ~/.local/share/IMPAKT (with
# CHIMERA's bank), a command (~/.local/bin/impakt) and a menu entry. Run it again to update.
set -e
cd "$(dirname "$0")"
VST3="$HOME/.vst3"; SHARE="$HOME/.local/share/IMPAKT"; BIN="$HOME/.local/bin"; APPS="$HOME/.local/share/applications"
mkdir -p "$VST3" "$SHARE" "$BIN" "$APPS"
rm -rf "$VST3/IMPAKT.vst3" "$SHARE/IMPAKT" "$SHARE/bank"
cp -a IMPAKT.vst3 "$VST3/IMPAKT.vst3"
cp -a app/IMPAKT app/bank "$SHARE/"
ln -sf "$SHARE/IMPAKT" "$BIN/impakt"
ICONS="$HOME/.local/share/icons/hicolor/256x256/apps"; mkdir -p "$ICONS"; cp impakt.png "$ICONS/impakt.png"
sed "s|@EXEC@|$SHARE/IMPAKT|" impakt.desktop > "$APPS/impakt.desktop"
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" >/dev/null 2>&1 || true
echo "IMPAKT installed: the VST3 in $VST3, the app in $SHARE (command: impakt)."
# what the interface needs (WebKitGTK) and the network (curl)
missing=""
ldconfig -p 2>/dev/null | grep -q "libwebkit2gtk-4\.[01]" || missing="$missing libwebkit2gtk-4.1-0"
ldconfig -p 2>/dev/null | grep -q "libcurl\.so\.4\|libcurl-gnutls\.so\.4" || missing="$missing libcurl4"
[ -n "$missing" ] && echo "To show its interface IMPAKT also needs:$missing (e.g. sudo apt install$missing)."
exit 0
