#! /bin/bash


if [[ ! -x vcpkg/vcpkg ]] ; then
  vcpkg/bootstrap-vcpkg.sh
fi

vcpkg/vcpkg --overlay-triplets=$PWD/../vcpkg-overlay/triplets install \
  rtmidi rtaudio qtbase[core,widgets,doubleconversion,concurrent]


cmake --preset ninja-vcpkg
cmake --build --preset ninja-vcpkg-release --target clap-host
tar -cvJf clap-host.tar.xz --strip-components=4 builds/ninja-vcpkg/host/Release/clap-host
