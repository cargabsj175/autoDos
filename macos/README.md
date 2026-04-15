# AutoDOS for macOS

macOS-native command-line and GUI versions of AutoDOS — play MS-DOS games on your Mac!

**No Homebrew required for end users.** The distributed DMG includes everything needed.

---

## Credits & Origin

### Original Project

**AutoDOS** was created by **[makuka97](https://github.com/makuka97)** as a Windows-only application written in C/C++. The original project provides a simple way to play hundreds of MS-DOS games by automatically detecting game executables and configuring DOSBox.

### macOS Port

This macOS port was developed by **[cargabsj175](https://github.com/cargabsj175)** using **vibe coding** — an iterative, AI-assisted development approach where the port was built conversationally, testing and refining each component until everything worked correctly on macOS.

### Key Principle

**Zero modifications to the original codebase.** All macOS-specific code lives in the `macos/` directory. The original `src/` folder and root project files remain completely untouched.

---

## Quick Start (For Users)

### Download & Install

1. **Download** `AutoDOS-v1.0.0-macOS.dmg`
2. **Open the DMG** (double-click)
3. **Drag** `AutoDOS.app` to your `Applications` folder
4. **Launch** AutoDOS from Applications

That's it! DOSBox is already included. No setup needed.

### Using AutoDOS

**GUI Mode:**
- Open `AutoDOS.app` from Applications
- Drag & drop DOS game `.zip`, `.7z`, or `.rar` files onto the window
- Double-click a game to launch

**CLI Mode (Advanced):**
```bash
# Access the CLI tool inside the .app bundle
/Applications/AutoDOS.app/Contents/Resources/AutoDOS-cli help

# List games
/Applications/AutoDOS.app/Contents/Resources/AutoDOS-cli list

# Add a game
/AppApplications/AutoDOS.app/Contents/Resources/AutoDOS-cli add ~/Downloads/game.zip
```

---

## Building from Source (For Developers)

### Prerequisites

```bash
# Install build dependencies via Homebrew
brew install cmake nlohmann-json sdl2 sdl2_ttf imagemagick
```

### Clean Build from Scratch

If you want to start completely fresh (removes all build artifacts):

```bash
# From the macos/ directory
cd macos

# Remove build directory, dist, and any staging files
rm -rf build dist staging

# Reconfigure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Test
./bin/AutoDOS-cli help
./bin/AutoDOS-gui
```

### Full Clean Rebuild (including downloaded dependencies)

```bash
cd macos

# Remove everything generated
rm -rf build dist staging AutoDOS.iconset AutoDOS.icns

# Fresh build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Build Distribution DMG

Create a self-contained `.dmg` with DOSBox bundled (no Homebrew needed for end users):

```bash
cd macos
./build-dmg.sh

# Output: dist/AutoDOS-v1.0.0-macOS.dmg
```

The script:
1. ✅ Builds AutoDOS CLI and GUI
2. ✅ Downloads DOSBox-Staging universal binary (Intel + Apple Silicon)
3. ✅ Converts `autodos.jpg` to `.icns` using ImageMagick
4. ✅ Creates `.app` bundle with everything included
5. ✅ Packages into a compressed `.dmg` for distribution

**Result:** An 11MB DMG that works on any Mac (Intel & M1/M2/M3) without installing anything.

---

## How the Port Was Developed

### Architecture

The macOS port follows a layered architecture to keep the original code untouched:

```
macos/
├── autodos.cpp              # Cross-platform core (adapted from original)
├── autodos.h                # Public API (copied from original src/)
├── miniz.c / miniz.h        # ZIP extraction (copied from original src/)
├── CMakeLists.txt           # Build configuration for macOS
├── build-dmg.sh             # DMG packaging script
│
├── cli/
│   └── main.cpp             # Command-line interface
│
├── gui/
│   ├── main.cpp             # SDL2 + ImGui GUI
│   ├── imgui.cpp            # ImGui core (from GitHub)
│   ├── imgui_*.cpp/h        # ImGui SDL2 backend
│   └── ...                  # Other ImGui files
│
├── shared/
│   ├── platform.cpp         # macOS/POSIX abstraction layer
│   └── platform.h           # Cross-platform interface
│
└── dist/
    └── AutoDOS-v1.0.0-macOS.dmg  # Final distributable
```

### Key Development Challenges Solved

#### 1. Windows API Replacement
The original used Win32 APIs (`CreateProcess`, `SHGetFolderPath`, `FindFirstFile`, etc.). Replaced with:
- POSIX `fork/exec` for process launching
- `getenv("HOME")` + `~/Library/Application Support/` for app data
- `opendir/readdir` for directory scanning
- Standard C++ `<filesystem>` for file operations

#### 2. Audio Issues
Initially launched DOSBox via `open -a` which broke audio:
```bash
# ❌ Audio broken
open -a "DOSBox Staging.app" --args -conf config.conf
```

Fixed by launching the binary directly from its working directory:
```bash
# ✅ Audio works
cd "/path/to/DOSBox Staging.app/Contents/MacOS"
./dosbox -conf config.conf
```

#### 3. Keyboard Focus Issues
After fixing audio, keyboard input stopped working. Resolved by:
- Using `setsid()` to create a new process session
- Properly setting the working directory before exec
- Avoiding `open -a` which caused focus loss

#### 4. Icon Generation
macOS requires `.icns` format with multiple resolutions. Solved with ImageMagick:
```bash
magick autodos.jpg -gravity center -background none -extent 1:1 /tmp/square.png
magick /tmp/square.png -resize 16x16 icon_16x16.png
# ... (10 sizes total)
iconutil -c icns AutoDOS.iconset  # Creates valid .icns
```

#### 5. DOSBox Path Detection
The code auto-detects where DOSBox is located:
```cpp
if (exeDir.find(".app/Contents/MacOS") != string::npos) {
    // Inside .app bundle — find DOSBox inside Contents/dosbox/
    g_dosboxPath = appContents + "/Contents/dosbox/DOSBox Staging.app/Contents/MacOS/dosbox";
} else {
    // Development directory
    g_dosboxPath = exeDir + "/dosbox/dosbox";
}
```

### Vibe Coding Process

This port was built using an iterative conversation-based approach:
1. Agent proposes a solution
2. Code is written and tested immediately
3. Issues are reported and fixed in the next iteration
4. Each step verified with actual builds and tests
5. No files modified in the original project

This approach allowed rapid prototyping while maintaining code quality and ensuring the original codebase remained pristine.

---

## Usage

### CLI Version

```bash
# Show help
./bin/AutoDOS-cli help

# List all games in library
./bin/AutoDOS-cli list

# Analyze a DOS game zip (without adding)
./bin/AutoDOS-cli analyze /path/to/game.zip

# Add a game to library
./bin/AutoDOS-cli add /path/to/game.zip

# Launch a game (use the ID from 'list' command)
./bin/AutoDOS-cli launch g1234567890_0

# Remove a game from library
./bin/AutoDOS-cli remove g1234567890_0
```

### GUI Version

```bash
./bin/AutoDOS-gui
```

**Controls:**
- **Add game**: Drag & drop `.zip`, `.7z`, or `.rar` onto the window
- **Launch game**: Double-click or select + click "Launch"
- **Delete game**: Select + click "Remove"
- **Scroll**: Mouse wheel or trackpad

---

## How It Works

1. **Drop a DOS game zip** onto the app or add via CLI
2. **AutoDOS analyzes** the archive:
   - Checks against known games database (`games.json`)
   - Falls back to intelligent scoring algorithm
   - Identifies the correct executable automatically
3. **Extracts and configures** the game with optimal settings
4. **Launch DOSBox** — game runs with the right configuration

### Game Detection Layers

1. **Database lookup** — Known games matched by filename fingerprint
2. **Batch launcher** — Detects `start.bat`, `run.bat`, etc.
3. **Intelligent scorer** — Ranks executables by name, size, location

---

## Platform Support

- ✅ **macOS** (Intel & Apple Silicon) — This port
- 🔄 **Linux** (possible with minor adjustments)
- ❌ **Windows** (use the original version by makuka97)

---

## Troubleshooting

### App won't launch (Gatekeeper warning)

macOS may warn about an unidentified developer. To fix:

1. **Right-click** (or Control-click) on `AutoDOS.app`
2. Select **Open**
3. Click **Open** again in the dialog

Or from terminal:
```bash
xattr -d com.apple.quarantine /Applications/AutoDOS.app
```

### Build fails with SDL2 errors

Make sure SDL2 is installed via Homebrew:

```bash
brew install sdl2 sdl2_ttf
```

If CMake can't find SDL2:

```bash
cmake .. -DSDL2_DIR=$(brew --prefix sdl2) -DSDL2_TTF_DIR=$(brew --prefix sdl2_ttf)
```

### Game doesn't launch

Check the analysis output:

```bash
./bin/AutoDOS-cli analyze /path/to/game.zip
```

This shows what executable was detected and the configuration.

### Games library is empty

Copy `games.json` from the CARTRIDGE project or let AutoDOS auto-populate it as you add games.

### Keyboard not working in DOSBox

Make sure you're using the latest version. Earlier builds had a keyboard focus issue that was resolved by:
- Launching DOSBox from its own working directory
- Using `setsid()` to create a new process session
- Avoiding `open -a` which caused input focus loss

### No audio in DOSBox

Ensure DOSBox is being launched as a direct binary, not via `open -a`. This was fixed in the current version by executing the binary directly from its `.app/Contents/MacOS/` directory.

---

## Development

### Adding New Features

The codebase is organized in layers:

1. **`shared/platform.cpp`** — OS-specific operations (file I/O, launching apps)
2. **`autodos.cpp`** — Core game detection logic (cross-platform)
3. **`cli/main.cpp`** or **`gui/main.cpp`** — User interface

When adding features:
- Keep platform-specific code in `platform.cpp`
- The core `autodos.cpp` should remain cross-platform
- Test on both CLI and GUI versions

### Building in Debug Mode

```bash
cd macos
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Code Style

- C++20 with STL
- No platform-specific code in core (use `platform.cpp`)
- Follow existing naming conventions
- Keep comments minimal and focused on "why" not "what"

### Verifying Original Code is Untouched

```bash
cd /path/to/autoDos
git status --short

# Should only show:
# ?? .DS_Store
# ?? .qwen/
# ?? macos/
```

If `src/` or root files appear as modified, something went wrong. They should never be changed.

---

## License

Same as the main AutoDOS project by makuka97.

---

## Credits

### Original Project
- **Creator:** [makuka97](https://github.com/makuka97)
- **Language:** C/C++
- **Platform:** Windows
- **Repository:** Original AutoDOS project

### macOS Port
- **Developer:** [cargabsj175](https://github.com/cargabsj175)
- **Method:** Vibe coding (AI-assisted iterative development)
- **Principle:** Zero modifications to original codebase

### Third-Party Libraries
- **miniz:** Rich Geldreich — ZIP extraction
- **nlohmann/json:** JSON handling
- **ImGui:** Omar Cornut — GUI framework
- **SDL2:** Simple DirectMedia Layer
- **SDL2_ttf:** Font rendering for SDL2
- **ImageMagick:** Icon generation
- **DOSBox-Staging:** DOSBox Staging Team — DOS emulator

### Dependencies (Build Time)
- CMake
- nlohmann-json (Homebrew)
- SDL2 + SDL2_ttf (Homebrew)
- ImageMagick (Homebrew) — for icon generation

---

## Acknowledgments

Thanks to **makuka97** for creating the original AutoDOS project that makes playing DOS games effortless. This port extends that convenience to macOS users while respecting the original codebase.

The vibe coding approach proved that complex cross-platform ports can be built conversationally, testing each component iteratively until everything works correctly — audio, keyboard, file drag-and-drop, and game launching all verified step by step.
