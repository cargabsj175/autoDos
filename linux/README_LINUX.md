# AutoDOS Linux Port

This directory is a Linux-native port of the AutoDOS core. It does not modify
the original Win32 source tree.

## Build

Install dependencies:

```bash
sudo apt update
sudo apt install cmake g++ git nlohmann-json3-dev libglfw3-dev libgl1-mesa-dev dosbox
```

Build:

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
```

The binary is:

```bash
./build-linux/bin/AutoDOS
./build-linux/bin/AutoDOS-GUI
```

## Usage

Start the GLFW + Dear ImGui launcher:

```bash
./build-linux/bin/AutoDOS-GUI
```

You can drop ZIP files onto the window, or type a ZIP path and click Import.
Imported games are saved in `library.json` and can be launched or removed from
the list.

Analyze a ZIP without extracting it:

```bash
./build-linux/bin/AutoDOS analyze /path/to/game.zip
```

Import a ZIP into `~/.local/share/autodos/games/`:

```bash
./build-linux/bin/AutoDOS import /path/to/game.zip
```

Import and launch immediately:

```bash
./build-linux/bin/AutoDOS import /path/to/game.zip --launch
```

Launch an existing generated config:

```bash
./build-linux/bin/AutoDOS launch ~/.local/share/autodos/games/game.conf
```

Use a specific DOSBox binary:

```bash
./build-linux/bin/AutoDOS import /path/to/game.zip --dosbox /usr/bin/dosbox --launch
```

You can also set:

```bash
export AUTODOS_DOSBOX=/usr/bin/dosbox
```

For the GUI, you can override the data directory with:

```bash
export AUTODOS_DATA_DIR=/path/to/autodos-data
./build-linux/bin/AutoDOS-GUI
```

## Data Files

By default, the port uses:

```text
~/.local/share/autodos/games.json
~/.local/share/autodos/library.json
~/.local/share/autodos/imgui.ini
~/.local/share/autodos/games/
```

Override those paths with:

```bash
--db /path/to/games.json
--data-dir /path/to/data-dir
```

For the GUI use `AUTODOS_DATA_DIR`; for DOSBox use `AUTODOS_DOSBOX`.
