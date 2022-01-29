# WARNING UNDER CONSTRUCTION!

This is not ready yet. Pass your way unless you know what your are doing.

# Minimal Clap Host

This repo serves as an example to demonstrate how to create a CLAP host.

## Building on various platforms

### macOS with brew

This options is the quickest. But has it dynamically links with Qt,
you'll have troubles with plugins being dynamically linked to Qt as well.

Note that the resulting build should not be distributed.

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

This option takes the longuest to build as it requires to build Qt.
Even if Qt is built statically and all symbols are hidden, there will still
be a symbol clash due to objective-c's runtime which registers all classes
into a flat namespace. For that we must rely upon `QT_NAMESPACE` which puts
every symbols from Qt in the namespace specified by `QT_NAMESPACE`.

VCPKG [PR](https://github.com/microsoft/vcpkg/pull/22713).

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

### Linux

### Using vcpkg

```bash
git clone --recurse-submodules https://github.com/free-audio/clap-host
cd clap-host
scripts/build.sh
```