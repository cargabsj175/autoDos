#!/bin/bash
# build-dmg.sh — Create a self-contained AutoDOS.app with DOSBox bundled
#
# AutoDOS - Original by makuka97 (https://github.com/makuka97)
# macOS port by cargabsj175 (vibe coding approach)
#
# No Homebrew dependencies needed for end users.
# This script builds everything into a distributable .dmg file.
#
# Usage: ./build-dmg.sh [output-dir]

set -e

OUTPUT_DIR="${1:-$(pwd)/dist}"
VERSION="1.0.0"
APP_NAME="AutoDOS"
BUNDLE_ID="com.autodos.mac"
DOSBOX_VERSION="0.82.2"
DOSBOX_URL="https://github.com/dosbox-staging/dosbox-staging/releases/download/v${DOSBOX_VERSION}/dosbox-staging-macOS-v${DOSBOX_VERSION}.dmg"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
STAGING_DIR="${SCRIPT_DIR}/staging"

echo "╔══════════════════════════════════════════════════════╗"
echo "║        AutoDOS macOS DMG Builder                     ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Build AutoDOS binaries ────────────────────────────────────────────
echo "▸ Step 1: Building AutoDOS binaries..."
cd "${SCRIPT_DIR}"

if [ ! -d "build" ]; then
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
else
    cd build
fi

cmake --build . --target AutoDOS-cli
cmake --build . --target AutoDOS-gui

echo "  ✓ AutoDOS-cli:  $(ls -lh bin/AutoDOS-cli | awk '{print $5}')"
echo "  ✓ AutoDOS-gui:  $(ls -lh bin/AutoDOS-gui | awk '{print $5}')"
echo ""

# ── Step 2: Download and extract DOSBox-Staging ───────────────────────────────
echo "▸ Step 2: Downloading DOSBox-Staging v${DOSBOX_VERSION}..."

DOSBOX_DMG="${STAGING_DIR}/dosbox-staging.dmg"
DOSBOX_APP="${STAGING_DIR}/DOSBox Staging.app"

if [ ! -f "${DOSBOX_APP}/Contents/MacOS/dosbox" ]; then
    mkdir -p "${STAGING_DIR}"
    
    if [ ! -f "${DOSBOX_DMG}" ]; then
        echo "  ↓ Downloading DOSBox-Staging..."
        curl -L -o "${DOSBOX_DMG}" "${DOSBOX_URL}"
    fi
    
    echo "  ⊘ Extracting DOSBox-Staging.app..."
    # Mount DMG
    MOUNT_POINT=$(hdiutil attach "${DOSBOX_DMG}" -nobrowse -readonly | grep -o '/Volumes/.*')
    
    # Copy .app
    if [ -d "${MOUNT_POINT}/DOSBox Staging.app" ]; then
        cp -R "${MOUNT_POINT}/DOSBox Staging.app" "${STAGING_DIR}/"
    elif [ -d "${MOUNT_POINT}/DOSBox\ Staging.app" ]; then
        cp -R "${MOUNT_POINT}/DOSBox\ Staging.app" "${STAGING_DIR}/"
    fi
    
    # Unmount
    hdiutil detach "${MOUNT_POINT}" -quiet
    
    echo "  ✓ DOSBox-Staging extracted"
else
    echo "  ✓ DOSBox-Staging already available"
fi
echo ""

# ── Step 3: Create .app bundle ────────────────────────────────────────────────
echo "▸ Step 3: Creating AutoDOS.app bundle..."

APP_BUNDLE="${STAGING_DIR}/${APP_NAME}.app"
CONTENTS="${APP_BUNDLE}/Contents"
MACOS="${CONTENTS}/MacOS"
RESOURCES="${CONTENTS}/Resources"

# Clean and create structure
rm -rf "${APP_BUNDLE}"
mkdir -p "${MACOS}"
mkdir -p "${RESOURCES}"
mkdir -p "${CONTENTS}/dosbox"

# Copy AutoDOS GUI as main executable
echo "  ⊕ Copying AutoDOS-gui..."
cp "${BUILD_DIR}/bin/AutoDOS-gui" "${MACOS}/${APP_NAME}"
chmod +x "${MACOS}/${APP_NAME}"

# Copy CLI tool as well (accessible via Resources)
cp "${BUILD_DIR}/bin/AutoDOS-cli" "${RESOURCES}/"
chmod +x "${RESOURCES}/AutoDOS-cli"

# Copy DOSBox into bundle
echo "  ⊕ Bundling DOSBox..."
if [ -d "${DOSBOX_APP}" ]; then
    cp -R "${DOSBOX_APP}" "${CONTENTS}/dosbox/"
fi

# Copy games.json if available
if [ -f "${SCRIPT_DIR}/../src/games.json" ]; then
    echo "  ⊕ Copying games.json..."
    cp "${SCRIPT_DIR}/../src/games.json" "${RESOURCES}/"
fi

# Convert icon from autodos.jpg to ICNS using ImageMagick
echo "  ⊕ Converting icon to ICNS..."
if [ -f "${SCRIPT_DIR}/autodos.jpg" ]; then
    cd "${SCRIPT_DIR}"
    
    # Create iconset directory
    rm -rf AutoDOS.iconset
    mkdir AutoDOS.iconset
    
    # Make image square first
    magick autodos.jpg -gravity center -background none -extent 1:1 /tmp/autodos_square.png 2>/dev/null
    
    # Generate all required sizes
    magick /tmp/autodos_square.png -resize 16x16   AutoDOS.iconset/icon_16x16.png
    magick /tmp/autodos_square.png -resize 32x32   AutoDOS.iconset/icon_16x16@2x.png
    magick /tmp/autodos_square.png -resize 32x32   AutoDOS.iconset/icon_32x32.png
    magick /tmp/autodos_square.png -resize 64x64   AutoDOS.iconset/icon_32x32@2x.png
    magick /tmp/autodos_square.png -resize 128x128 AutoDOS.iconset/icon_128x128.png
    magick /tmp/autodos_square.png -resize 256x256 AutoDOS.iconset/icon_128x128@2x.png
    magick /tmp/autodos_square.png -resize 256x256 AutoDOS.iconset/icon_256x256.png
    magick /tmp/autodos_square.png -resize 512x512 AutoDOS.iconset/icon_256x256@2x.png
    magick /tmp/autodos_square.png -resize 512x512 AutoDOS.iconset/icon_512x512.png
    magick /tmp/autodos_square.png -resize 1024x1024 AutoDOS.iconset/icon_512x512@2x.png
    
    # Convert to ICNS
    if iconutil -c icns AutoDOS.iconset 2>/dev/null; then
        cp AutoDOS.icns "${RESOURCES}/AutoDOS.icns"
        ICON_SIZE=$(ls -lh AutoDOS.icns | awk '{print $5}')
        echo "    ✓ Icon created (${ICON_SIZE})"
    else
        echo "    ⚠ Icon conversion failed"
    fi
    
    # Cleanup
    rm -rf AutoDOS.iconset AutoDOS.icns /tmp/autodos_square.png
    cd "${SCRIPT_DIR}"
else
    echo "  ⚠ No autodos.jpg found"
fi

# Create Info.plist
echo "  ⊕ Creating Info.plist..."
cat > "${CONTENTS}/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${APP_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>AutoDOS</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2026 AutoDOS. All rights reserved.</string>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>zip</string>
                <string>7z</string>
                <string>rar</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>DOS Game Archive</string>
            <key>CFBundleTypeRole</key>
            <string>Viewer</string>
            <key>LSHandlerRank</key>
            <string>Default</string>
        </dict>
    </array>
</dict>
</plist>
PLIST

# Calculate bundle size
BUNDLE_SIZE=$(du -sh "${APP_BUNDLE}" | awk '{print $1}')
echo "  ✓ AutoDOS.app created (${BUNDLE_SIZE})"
echo ""

# ── Step 4: Create DMG ───────────────────────────────────────────────────────
echo "▸ Step 4: Creating distributable DMG..."

DMG_NAME="${APP_NAME}-v${VERSION}-macOS.dmg"
DMG_PATH="${OUTPUT_DIR}/${DMG_NAME}"

mkdir -p "${OUTPUT_DIR}"

# Remove old DMG
rm -f "${DMG_PATH}"

# Create temporary directory for DMG contents
DMG_STAGING="${STAGING_DIR}/dmg"
rm -rf "${DMG_STAGING}"
mkdir -p "${DMG_STAGING}"

# Copy .app
cp -R "${APP_BUNDLE}" "${DMG_STAGING}/"

# Create Applications symlink
ln -s /Applications "${DMG_STAGING}/Applications"

# Create .background directory for DMG customization
mkdir -p "${DMG_STAGING}/.background"

# Calculate DMG size (add 20% overhead)
DMG_SIZE=$(du -sm "${DMG_STAGING}" | awk '{print int($1 * 1.3)}')

echo "  ⊘ Creating ${DMG_NAME} (${DMG_SIZE}MB)..."

# Create DMG with compression
hdiutil create \
    -volname "${APP_NAME} v${VERSION}" \
    -srcfolder "${DMG_STAGING}" \
    -ov \
    -format UDZO \
    -imagekey zlib-level=9 \
    -fs HFS+ \
    -size "${DMG_SIZE}m" \
    "${DMG_PATH}"

echo "  ✓ DMG created"
echo ""

# ── Step 5: Summary ───────────────────────────────────────────────────────────
echo "╔══════════════════════════════════════════════════════╗"
echo "║        Build Complete!                               ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Output: ${DMG_PATH}"
echo "Size:   $(ls -lh "${DMG_PATH}" | awk '{print $5}')"
echo ""
echo "Contents:"
echo "  • AutoDOS.app (GUI application)"
echo "  • DOSBox-Staging v${DOSBOX_VERSION} (bundled)"
echo "  • AutoDOS-cli (command-line tool inside .app)"
echo ""
echo "To install: Open the DMG and drag AutoDOS.app to Applications"
echo "To test:    Open ${DMG_PATH}"
echo ""

# Cleanup staging (keep build dir)
echo "▸ Cleaning up staging files..."
rm -rf "${STAGING_DIR}"
echo ""

echo "✅ Done!"
