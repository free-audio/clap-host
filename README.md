# WARNING UNDER CONSTRUCTION!

This is not ready yet. Pass your way unless you know what your are doing.

# Minimal Clap Host

This repo serves as an example to demonstrate how to create a CLAP host.

# Note on static build vs dynamic build

The host uses Qt for its GUI.
It is fine to dynamically link to Qt, but it is not for a plugin.
We offer two options:
- static build, cmake preset: `ninja-vcpkg` or `vs-vcpkg` on Windows.
- dynamic builg, cmake preset: `ninja-system`

Static builds are convenient for deployment as they are self containded.

Dynamic builds will get your started quickly if your system provides Qt6.
Static builds will require more time and space.

There is a pending VCPKG [PR](https://github.com/microsoft/vcpkg/pull/22713).

## Building on various platforms

### macOS, dynamic build with brew

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

### macOS with vcpkg

```shell
# Install build tools
brew install cmake ninja

# Checkout the code
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
scripts/build.sh
```

### Windows

#### Enable long path support

Make sure your system supports long paths. Run this in an administrator PowerShell:

```powershell
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

Reference: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=powershell

#### Build

Install **Visual Studio 2022**; you can install it from the **Microsoft Store**. It can also install **git** and **CMake** for you.

Use the following command inside `Developer PowerShell For VS 2022`:
```powershell
# Checkout the code very close to the root to avoid windows long path issues...
cd c:\
git clone --recurse-submodules https://github.com/free-audio/clap-host c-h
cd c-h

scripts/build.sh
```

### Linux, using system libraries (dynamic)

```bash
# on archlinux, adapt to your distribution and package manager
sudo pacman -S qt git ninja cmake

git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
cmake --preset ninja-system
cmake --build --preset ninja-system
```

### Linux, using vcpkg (static)

```bash
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
scripts/build.sh
```