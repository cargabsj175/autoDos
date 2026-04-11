# AutoDOS — Build Instructions

## Project Structure

```
AutoDOS/
├── src/
│   ├── main.cpp          ← Raylib GUI
│   ├── autodos.cpp       ← Core engine
│   ├── autodos.h         ← Public interface
│   ├── miniz.h           ← ZIP library (download separately)
│   ├── miniz.c           ← ZIP library (download separately)
│   └── games.json        ← Game database (copy from CARTRIDGE/rom/)
├── dosbox/
│   └── dosbox.exe        ← DOSBox Staging (copy from CARTRIDGE/native/dosbox-staging-win/)
├── assets/
│   └── icon.rc           ← (optional) Windows icon
├── CMakeLists.txt
└── BUILD.md
```

## Prerequisites

- **Visual Studio 2022** (or 2019) with C++ workload
- **CMake 3.20+**
- **Git** (for FetchContent to download Raylib + nlohmann/json)

## Step 1 — Get miniz

Download the single-file release from:
https://github.com/richgel999/miniz/releases

Download `miniz.h` and `miniz.c`, place both in `src/`.

## Step 2 — Copy games.json

Copy `games.json` from your CARTRIDGE install into `src/`:
```
C:\cartridge\cartridge-desktop\CARTRIDGE-v0.4.1 (39)\cartridge\rom\games.json
→ AutoDOS\src\games.json
```

## Step 3 — Copy DOSBox

Copy the entire `dosbox-staging-win` folder from CARTRIDGE into `dosbox/`:
```
C:\cartridge\cartridge-desktop\CARTRIDGE-v0.4.1 (39)\cartridge\native\dosbox-staging-win\
→ AutoDOS\dosbox\
```

## Step 4 — Build

Open **Developer Command Prompt for VS 2022**, then:

```cmd
cd C:\path\to\AutoDOS
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

The executable will be at `build\bin\Release\AutoDOS.exe`.

## Step 5 — Run

```cmd
build\bin\Release\AutoDOS.exe
```

On first run, `games.json` is copied to `%APPDATA%\AutoDOS\`.
Drop any DOS zip onto the window. AutoDOS handles the rest.

## How it works

1. Drop a `.zip` (or `.7z`, `.rar`) onto the window
2. AutoDOS fingerprints the filename → checks `games.json`
3. Known game → exact config applied (cycles, memory, CD mount)
4. Unknown game → scorer finds the right exe automatically
5. DOSBox launches silently
6. Unknown games are auto-added to the database after launch

## Controls

| Action              | Input                        |
|---------------------|------------------------------|
| Add game            | Drag & drop zip onto window  |
| Launch game         | Double-click or Enter        |
| Select              | Single click or arrow keys   |
| Delete game         | Del key or X button          |
| Scroll list         | Mouse wheel                  |

## Adding to games.json

Use the Python scraper to add games:
```cmd
python autodos_scraper.py --add --title "Dune II" --exe "DUNE2.EXE" --cycles 5000
```

Then copy the updated `games.json` back to the AutoDOS `src/` folder and rebuild,
or drop it directly into `%APPDATA%\AutoDOS\games.json` for immediate effect.
