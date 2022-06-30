<br/><a href="https://repology.org/project/clap-host/versions" target="_blank" rel="noopener" title="Packaging status"><img src="https://repology.org/badge/vertical-allrepos/clap-host.svg"></a>

- [Minimal Clap Host](#minimal-clap-host)
- [Notes on static build vs dynamic build](#notes-on-static-build-vs-dynamic-build)
  - [Building on macOS](#building-on-macos)
    - [dynamic build using brew (recommended)](#dynamic-build-using-brew-recommended)
    - [static build using vcpkg](#static-build-using-vcpkg)
  - [Building on Windows](#building-on-windows)
    - [Enable long path support](#enable-long-path-support)
    - [static build](#static-build)
  - [Building on Linux](#building-on-linux)
    - [dynamic build using system libraries](#dynamic-build-using-system-libraries)
    - [static build using vcpkg](#static-build-using-vcpkg-1)

# Minimal Clap Host

This repo serves as an example to demonstrate how to create a CLAP host.

# Notes on static build vs dynamic build

The host uses Qt for its GUI.
It is fine to dynamically link to Qt, but it is not for a plugin.
We offer two options:
- static build, cmake preset: `ninja-vcpkg` or `vs-vcpkg` on Windows.
- dynamic builg, cmake preset: `ninja-system`

Static builds are convenient for deployment as they are self containded.

Dynamic builds will get your started quickly if your system provides Qt6.
Static builds will require more time and space.

## Building on macOS

### dynamic build using brew (recommended)

```shell
# Install dependencies
brew install qt6 pkgconfig rtaudio rtmidi ninja cmake

# Checkout the code
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host

# Build
cmake --preset ninja-system
cmake --build --preset ninja-system
```

### static build using vcpkg

```shell
# Install build tools
brew install cmake ninja

# Checkout the code
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
scripts/build.sh
```

## Building on Windows

### Enable long path support

Make sure your system supports long paths. Run this in an administrator PowerShell:

```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

Reference: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=powershell

### static build

Install **Visual Studio 2022**; you can install it from the **Microsoft Store**. It can also install **git** and **CMake** for you.

Use the following command inside `Developer PowerShell For VS 2022`:
```powershell
# Checkout the code very close to the root to avoid windows long path issues...
cd c:\
git clone --recurse-submodules https://github.com/free-audio/clap-host c-h
cd c-h

scripts/build.sh
```

## Building on Linux

### dynamic build using system libraries

```bash
# on archlinux, adapt to your distribution and package manager
sudo pacman -S qt git ninja cmake rtaudio rtmidi

git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
cmake --preset ninja-system
cmake --build --preset ninja-system
```

### static build using vcpkg

```bash
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
scripts/build.sh
```
