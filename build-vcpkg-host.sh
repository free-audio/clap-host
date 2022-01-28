#! /bin/bash

cd vcpkg
if [[ ! -x vcpkg ]] ; then
  ./bootstrap-vcpkg.sh
fi

./vcpkg --overlay-triplets=$PWD/../vcpkg-overlay/triplets --triplet=arm64-osx-clap-host install --recurse rtmidi rtaudio qtbase[core,widgets,doubleconversion,concurrent]
cd ..
cmake --preset macos-arm64-host