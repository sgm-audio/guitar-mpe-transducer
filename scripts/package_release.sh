#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

VERSION="${VERSION:-$(sed -n 's/^[[:space:]]*VERSION[[:space:]]*\([0-9.]*\).*/\1/p' CMakeLists.txt | head -n1)}"
VERSION="${VERSION:-0.1.0}"

case "$(uname -s)" in
    Darwin) OS_NAME="macos" ;;
    Linux)  OS_NAME="linux" ;;
    MINGW*|MSYS*|CYGWIN*) OS_NAME="windows" ;;
    *)      OS_NAME="unknown" ;;
esac

BUILD_JUCE="${TRANSDUCER_BUILD_JUCE:-OFF}"
BUILD_JUCE_CLAP="${TRANSDUCER_BUILD_JUCE_CLAP:-OFF}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release}"
DIST_DIR="$ROOT_DIR/dist"
PKG_NAME="guitar-mpe-transducer-${VERSION}-${OS_NAME}"
STAGE_DIR="$DIST_DIR/$PKG_NAME"
ZIP_PATH="$DIST_DIR/${PKG_NAME}.zip"

echo "==> Configuring ($PKG_NAME, TRANSDUCER_BUILD_JUCE=$BUILD_JUCE)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DTRANSDUCER_BUILD_JUCE="$BUILD_JUCE" \
    -DTRANSDUCER_BUILD_JUCE_CLAP="$BUILD_JUCE_CLAP"

echo "==> Building Release CLAP + tests"
cmake --build "$BUILD_DIR" --config Release --target guitar_mpe_transducer transducer_tests --parallel
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure

if [ "$BUILD_JUCE" = "ON" ]; then
    JUCE_TARGETS="guitar_mpe_transducer_juce_VST3 guitar_mpe_transducer_juce_Standalone"
    [ "$OS_NAME" = "macos" ] && JUCE_TARGETS="$JUCE_TARGETS guitar_mpe_transducer_juce_AU"
    [ "$BUILD_JUCE_CLAP" = "ON" ] && JUCE_TARGETS="$JUCE_TARGETS guitar_mpe_transducer_juce_CLAP"
    echo "==> Building JUCE targets: $JUCE_TARGETS"
    cmake --build "$BUILD_DIR" --config Release --target $JUCE_TARGETS --parallel
fi

rm -rf "$STAGE_DIR" "$ZIP_PATH"
mkdir -p "$STAGE_DIR/clap"

# Copy native headless CLAP binary/bundle
find "$BUILD_DIR" -maxdepth 4 -name "guitar_mpe_transducer.clap" -prune -exec cp -R {} "$STAGE_DIR/clap/" \;

# Copy JUCE bundles if built
if [ "$BUILD_JUCE" = "ON" ]; then
    mkdir -p "$STAGE_DIR/vst3" "$STAGE_DIR/standalone"
    find "$BUILD_DIR" -name "*.vst3" -prune -exec cp -R {} "$STAGE_DIR/vst3/" \;
    if [ "$BUILD_JUCE_CLAP" = "ON" ]; then
        find "$BUILD_DIR" -path "*CLAP*" -name "*.clap" -prune -exec cp -R {} "$STAGE_DIR/clap/" \;
    fi
    find "$BUILD_DIR" -path "*Standalone*" \( -name "Guitar MPE Transducer*" -o -name "*.app" -o -name "*.exe" \) -maxdepth 6 -prune -exec cp -R {} "$STAGE_DIR/standalone/" \;
    if [ "$OS_NAME" = "macos" ]; then
        mkdir -p "$STAGE_DIR/au"
        find "$BUILD_DIR" -name "*.component" -prune -exec cp -R {} "$STAGE_DIR/au/" \;
    fi
fi

# Copy presets & docs
cp -R "$ROOT_DIR/presets" "$STAGE_DIR/presets"
cp "$ROOT_DIR/LICENSE" "$ROOT_DIR/README.md" "$STAGE_DIR/"

# Generate RELEASE_NOTES.txt with changelog excerpt and one-line install notes
CHANGELOG="$(git log -n 10 --oneline 2>/dev/null || echo "Initial release")"
cat > "$STAGE_DIR/RELEASE_NOTES.txt" <<EOF
guitar-mpe-transducer v${VERSION} (${OS_NAME})
=============================================

Install Notes (unsigned / unnotarized local build):
- CLAP:       Copy clap/*.clap to ~/.clap/ (Linux), ~/Library/Audio/Plug-Ins/CLAP/ (macOS), or %COMMONPROGRAMFILES%\\CLAP\\ (Windows).
              ("Guitar MPE Transducer.clap" includes the JUCE GUI; "guitar_mpe_transducer.clap" is the headless native CLAP.)
- VST3:       Copy vst3/*.vst3 to ~/.vst3/ (Linux), ~/Library/Audio/Plug-Ins/VST3/ (macOS), or %COMMONPROGRAMFILES%\\VST3\\ (Windows).
- AU (macOS): Copy au/*.component to ~/Library/Audio/Plug-Ins/Components/ and run \`killall -9 AudioComponentRegistrar\` if needed.
- Standalone: Run the binary/app in standalone/ directly (no installation required).
- Presets:    Load presets/*.xml (GTRJ v1) via the JUCE preset loader or host program menu.

Recent Changes:
${CHANGELOG}
EOF

echo "==> Creating archive: $ZIP_PATH"
if command -v zip >/dev/null 2>&1; then
    (cd "$DIST_DIR" && zip -qr "$(basename "$ZIP_PATH")" "$PKG_NAME")
else
    python3 -c "
import os, zipfile
root = '$STAGE_DIR'
with zipfile.ZipFile('$ZIP_PATH', 'w', zipfile.ZIP_DEFLATED) as zf:
    for dirpath, _, filenames in os.walk(root):
        for f in filenames:
            full = os.path.join(dirpath, f)
            zf.write(full, os.path.relpath(full, '$DIST_DIR'))
"
fi

echo "==> Done: $ZIP_PATH"
