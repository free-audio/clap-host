#! /bin/bash

cd vcpkg
if [[ ! -x vcpkg ]] ; then
  ./bootstrap-vcpkg.sh
fi

# qtbase[core,widgets,doubleconversion,concurrent]
./vcpkg --overlay-triplets=$PWD/../vcpkg-overlay/triplets install \
  rtmidi rtaudio

cd ..
cmake --preset ninja-vcpkg
cmake --build --preset ninja-vcpkg-release --target clap-host
tar -cvJf clap-host.tar.xz --strip-components=4 builds/ninja-vcpkg/host/Release/clap-host
