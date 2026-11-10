#!/bin/bash
# IMPAKT: builds the Mac plugins (VST3, AU + standalone app). Double-click it in the Finder, with the DAW closed.
# Everything is written to build_mac.log next to this file.
cd "$(dirname "$0")" || exit 1
ROOT="$PWD"
LOG="$ROOT/build_mac.log"
exec > >(tee "$LOG") 2>&1

finish() {
    echo
    echo "STATUS: $1"
    echo "(you can close this window)"
    sleep 1
    exit 0
}

echo "=== IMPAKT Mac build — $(date) ==="
sw_vers 2>/dev/null | tr '\n' ' '; echo; echo "arch: $(uname -m)"

# --- a DAW that has IMPAKT loaded crashes when the plugin files are replaced under it
while :; do
    RUNNING=""
    for daw in "Live" "Logic Pro" "Logic Pro X" "Bitwig Studio" "REAPER" "Reason" "Cubase" "Studio One" "MainStage"; do
        pgrep -xq "$daw" && RUNNING="$RUNNING [$daw]"
    done
    [ -z "$RUNNING" ] && break
    echo ">>> Please quit$RUNNING first, then press Enter here (or type s + Enter to build anyway)."
    read -r ans; [ "$ans" = "s" ] && break
done

# --- Xcode (the full app, not only the command line tools)
if [ -d /Applications/Xcode.app ] && ! xcode-select -p 2>/dev/null | grep -q "Xcode.*\.app"; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
if [ -z "$DEVELOPER_DIR" ] && ! xcode-select -p 2>/dev/null | grep -q "\.app"; then
    XA=$(ls -d /Applications/Xcode*.app 2>/dev/null | head -1)
    [ -n "$XA" ] && export DEVELOPER_DIR="$XA/Contents/Developer"
fi
echo "developer dir: ${DEVELOPER_DIR:-$(xcode-select -p 2>/dev/null)}"
if ! XV=$(xcodebuild -version 2>&1); then
    echo "$XV"
    if echo "$XV" | grep -qi licen; then
        finish "FAILED: Xcode licence not accepted. Open Xcode once and accept it, then run this again."
    fi
    finish "FAILED: Xcode not found. Install Xcode from the App Store, open it once, then run this again."
fi
echo "$XV"

# --- CMake: use the installed one, otherwise download a private copy into .tools/
CMAKE=$(command -v cmake)
for c in /opt/homebrew/bin/cmake /usr/local/bin/cmake /Applications/CMake.app/Contents/bin/cmake "$ROOT/.tools/cmake/CMake.app/Contents/bin/cmake"; do
    [ -z "$CMAKE" ] && [ -x "$c" ] && CMAKE="$c"
done
if [ -z "$CMAKE" ]; then
    echo "CMake not found: downloading CMake 4.4.4 into .tools/ (about 60 MB)…"
    mkdir -p "$ROOT/.tools/cmake"
    curl -fL --progress-bar https://github.com/Kitware/CMake/releases/download/v4.4.4/cmake-4.4.4-macos-universal.tar.gz -o "$ROOT/.tools/cmake.tgz" \
        || finish "FAILED: could not download CMake (no internet?)."
    tar -xzf "$ROOT/.tools/cmake.tgz" -C "$ROOT/.tools/cmake" --strip-components 1 || finish "FAILED: could not unpack CMake."
    CMAKE="$ROOT/.tools/cmake/CMake.app/Contents/bin/cmake"
fi
echo "cmake: $CMAKE ($("$CMAKE" --version | head -1))"

# --- configure (downloads JUCE the first time) and build
# Xcode generator first; if xcodebuild itself is broken, fall back to plain Makefiles with the same compiler.
NCPU=$(sysctl -n hw.ncpu)
XCODE_BROKEN=0
# An earlier build made a Mac AUv3 inside the app, registered with the same codes as the AU: some hosts
# (Ableton Live) then load that one instead, and crash. Unregister it and remove it before building.
LSREG=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
for ap in $(pluginkit -m -v -A 2>/dev/null | grep -i "impaktaudio" | grep -o '/[^[:space:]]*\.appex'); do
    echo "unregistering old AUv3: $ap"; pluginkit -r "$ap" 2>/dev/null
done
for app in build-mac/IMPAKT_artefacts/*/Standalone/IMPAKT.app; do
    [ -d "$app/Contents/PlugIns" ] && { "$LSREG" -u "$PWD/$app" >/dev/null 2>&1; rm -rf "$app"; echo "removed old app with AUv3: $app"; }
done
rm -rf build-mac/IMPAKT_artefacts/*/AUv3

echo; echo "=== configure (Xcode) ==="
if "$CMAKE" -S plugin -B build-mac -G Xcode -DIMPAKT_MAC_AUV3=OFF; then
    BDIR=build-mac
    echo; echo "=== build (Release) — a few minutes the first time ==="
    START=$(date +%s)
    "$CMAKE" --build build-mac --config Release -- -quiet -jobs "$NCPU" || finish "FAILED: build step (see the errors above)."
else
    grep -q "Error loading required libraries" build-mac/CMakeFiles/CMakeConfigureLog.yaml 2>/dev/null && XCODE_BROKEN=1
    [ $XCODE_BROKEN = 1 ] && echo ">>> xcodebuild cannot start: its system components do not match this Xcode (fix: sudo xcodebuild -runFirstLaunch)."
    echo; echo "=== configure (Makefiles) ==="
    "$CMAKE" -S plugin -B build-mac-make -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release || finish "FAILED: configure step (see above)."
    BDIR=build-mac-make
    echo; echo "=== build (Release) — a few minutes the first time ==="
    START=$(date +%s)
    "$CMAKE" --build build-mac-make -j "$NCPU" 2>&1 | grep -vE "^\[ *[0-9]+%\] (Building|Linking|Built)|^-- (Installing|Up-to-date)" ; [ ${PIPESTATUS[0]} = 0 ] || finish "FAILED: build step (see the errors above)."
fi
echo "build time: $(( $(date +%s) - START )) s"

# --- results
ART="$BDIR/IMPAKT_artefacts/Release"
echo; echo "=== products ==="
ls -d "$ART"/*/* 2>/dev/null
echo; echo "=== installed ==="
ls -ld ~/Library/Audio/Plug-Ins/VST3/IMPAKT.vst3 ~/Library/Audio/Plug-Ins/Components/IMPAKT.component 2>&1

# --- AU validation (refresh the AU cache first so the new component is seen)
echo; echo "=== auval ==="
killall -9 AudioComponentRegistrar >/dev/null 2>&1
sleep 2
auval -v aumu Imp8 Impk > "$ROOT/build_mac_auval.log" 2>&1
tail -n 6 "$ROOT/build_mac_auval.log"
NOTE=""
[ $XCODE_BROKEN = 1 ] && NOTE=" (Xcode is broken: run 'sudo xcodebuild -runFirstLaunch' before building for iPhone / iPad.)"
if grep -q "AU VALIDATION SUCCEEDED" "$ROOT/build_mac_auval.log"; then
    finish "OK — VST3 and AU installed, AU validation passed.$NOTE"
else
    finish "BUILT — VST3 and AU installed, but AU validation did not pass (see build_mac_auval.log).$NOTE"
fi
