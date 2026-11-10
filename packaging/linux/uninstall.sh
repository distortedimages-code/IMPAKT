#!/bin/sh
# Removes what install.sh put in place (the projects and settings stay: ~/.config/IMPAKT).
rm -rf "$HOME/.vst3/IMPAKT.vst3" "$HOME/.local/share/IMPAKT" "$HOME/.local/bin/impakt" "$HOME/.local/share/applications/impakt.desktop" "$HOME/.local/share/icons/hicolor/256x256/apps/impakt.png"
echo "IMPAKT removed."
