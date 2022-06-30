#! /bin/bash -e

cpu="$(uname -m)"
case "$cpu" in
x86_64)
  cpu="x64";;
i686)
  cpu="x86";;
esac

if [[ $(uname) = Linux ]] ; then
  QT_FEATURES=",xcb,xcb-xlib,xkb,xkbcommon-x11,xlib,xrender,fontconfig,harfbuzz,egl"
  cmake_preset="ninja-vcpkg"
  triplet=$cpu-linux
  buildtrees=vcpkg/buildtrees
  rtaudio_opts=alsa
  rtmidi_opts=alsa
elif [[ $(uname) = Darwin ]] ; then
  QT_FEATURES=",harfbuzz"
  cmake_preset="ninja-vcpkg"
  triplet=$cpu-osx
  buildtrees=vcpkg/buildtrees
  rtaudio_opts=
  rtmidi_opts=
else
  QT_FEATURES=""
  cmake_preset="vs-vcpkg"
  triplet=$cpu-win
  buildtrees="C:\B"
  rtaudio_opts=asio
  rtmidi_opts=
fi

if [[ "$1" != "" ]]; then
  triplet="$1"
fi

if [[ ! -x vcpkg/vcpkg ]] ; then
  vcpkg/bootstrap-vcpkg.sh
else
  vcpkg/vcpkg --overlay-triplets=vcpkg-overlay/triplets $vcpkg_option upgrade --no-dry-run --debug
fi

vcpkg_triplet="--triplet ${triplet}-clap-host --host-triplet ${triplet}-clap-host"
cmake_triplet="-DVCPKG_TARGET_TRIPLET=${triplet}-clap-host -DCMAKE_VCPKG_HOST_TRIPLET=${triplet}-clap-host"

vcpkg/vcpkg --overlay-triplets=vcpkg-overlay/triplets $vcpkg_triplet install --recurse \
  "rtmidi[${rtmidi_opts}]" "rtaudio[${rtaudio_opts}]" "qtbase[core,png,widgets,doubleconversion,concurrent,appstore-compliant,freetype${QT_FEATURES}]"

# save space
rm -rf vcpkg/buildtrees

vcpkg/vcpkg --overlay-triplets=vcpkg-overlay/triplets $vcpkg_options upgrade --debug

cmake --preset $cmake_preset $cmake_triplet
cmake --build --preset $cmake_preset --config Release --target clap-host
