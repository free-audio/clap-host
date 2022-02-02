#! /bin/bash

if [[ ! -x vcpkg/vcpkg ]] ; then
  vcpkg/bootstrap-vcpkg.sh
fi

if [[ $(uname) = Linux ]] ; then
  QT_FEATURES=",xcb,xcb-xlib,xkb,xkbcommon-x11,xlib,xrender,fontconfig,harfbuzz"
  cmake_preset="ninja-vcpkg"
  triplet=$(uname -m)-linux
elif [[ $(uname) = Darwin ]] ; then
  QT_FEATURES=",harfbuzz"
  cmake_preset="ninja-vcpkg"
  triplet=$(uname -m)-osx
else
  QT_FEATURES=""
  cmake_preset="vs-vcpkg"
  triplet=$(uname -m)-windows
fi

if [[ "$1" != "" ]]; then
  triplet="$1"
fi

vcpkg_triplet="--triplet ${triplet}-clap-host --host-triplet ${triplet}-clap-host"
cmake_triplet="-DVCPKG_TARGET_TRIPLET=${triplet}-clap-host -DCMAKE_VCPKG_HOST_TRIPLET=${triplet}-clap-host"

vcpkg/vcpkg --overlay-triplets=vcpkg-overlay/triplets $vcpkg_triplet install --recurse \
  rtmidi rtaudio "core,png,widgets,doubleconversion,concurrent,appstore-compliant,freetype${QT_FEATURES}]"

# save space
rm -rf vcpkg/buildtrees

cmake --preset $cmake_preset $cmake_triplet
cmake --build --preset $cmake_preset --config Release --target clap-plugins
