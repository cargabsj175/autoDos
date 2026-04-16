#AutoDOS

This repository maintains the original AutoDOS for Windows and adds separate forks for Linux and macOS.

The original README is preserved unchanged in `README_ORIGINAL.md`.

# What was done

- The original Windows project is preserved in the repository root.

- The Linux fork is added to `linux/`.

- The macOS fork is added to `macos/`.

- Build scripts for Windows, Linux, and macOS will be added.

- A script to compile the Windows binary from Linux using MinGW-w64 has been added.

- Cleanup scripts for POSIX and Windows environments have been added.

##Structure

```
AutoDOS/
├── src/ Original Windows Project
├── Linux/Linux Fork
├── macos/ macOS Fork
├── build-windows.ps1 Compiles Windows from Windows
├── build-windows-from-linux.sh Compiles Windows from Linux with MinGW-w64
├── build-linux.sh Compiles the Linux fork
├── build-macos.sh Compiles the macOS fork
├── clean.sh Cleans up Linux/macOS
├── clean.ps1 Cleans up Windows
├── README.md New main README
└── README_ORIGINAL.md README Original preserved
```

## Compiler on Windows

Requirements:

- Visual Studio 2022 or 2019 with the C++ workload.

- CMake.

- Go.


Command:

```powershell
.\build-windows.ps1
```

Compiling in Debug mode:

```powershell
.\build-windows.ps1 - Configuration debugging
```

Output:

```text
dist/AutoDOS.exe
```

## Compiler for Windows from Linux

Requirements:

```
sudo apt install cmake git mingw-w64
```

Command:

```
sudo ./build-windows-from-linux.sh
```

Compiling in Debug mode:

```
sudo ./build-windows-from-linux.sh Debugging
```

Output:

```
dist-windows/AutoDOS.exe
```

## Compiler on Linux

Requirements Recommended:

```
sudo update sudo apt
sudo apt install cmake g++ git nlohmann-json3-dev libglfw3-dev libgl1-mesa-dev dosbox
```

Command:

```
./build-linux.sh
```

Compiling in Debug mode:

```
./build-linux.sh Debug
```

Output:

```
linux/dist/
```

## Compile on macOS

Recommended requirements:

```tap
brew install cmake nlohmann-json sdl2 sdl2_ttf
```

Command:

```tap
./build-macos.sh
```

Compiling in mode Debug:

```tap
./build-macos.sh Debug
```

Build DMG:

```tap
./build-macos.sh Launch --dmg
```

Output:

```text
macos/dist/
```

## Clean up builds

Linux/macOS:

```
./clean.sh
```

Windows:

```
.\clean.ps1
```

Clean up scripts remove build and distribution artifacts:
`build/`, `dist/`, `linux/build-linux/`, `linux/dist/`, `macos/build/`,
`macos/dist/` and related temporary files.
