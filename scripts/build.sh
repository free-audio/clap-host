#! /bin/bash

if [[ ! -x vcpkg/vcpkg ]] ; then
  vcpkg/bootstrap-vcpkg.sh
fi

if [[ "$1" != "" ]]; then
  vcpkg_triplet="--triplet $1"
  cmake_triplet="-DVCPKG_TARGET_TRIPLET=$1"
fi

if [[ $(uname) = Linux ]] ; then
  QT_FEATURES=",xcb"
elif [[ $(uname) = Darwin ]] ; then
  QT_FEATURES=""
fi

vcpkg/vcpkg --overlay-triplets=vcpkg-overlay/triplets $vcpkg_triplet install \
  rtmidi rtaudio "qtbase[core,png,widgets,doubleconversion,concurrent,appstore-compliant${QT_FEATURES}]"

cmake --preset ninja-vcpkg $cmake_triplet
cmake --build --preset ninja-vcpkg-release --target clap-host
tar -cvJf clap-host.tar.xz --strip-components=4 builds/ninja-vcpkg/host/Release/clap-host
