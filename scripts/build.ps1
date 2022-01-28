cd vcpkg
bootstrap-vcpkg.bat

vcpkg --overlay-triplets=$PWD/../vcpkg-overlay/triplets install --recurse rtmidi rtaudio qtbase[core,widgets,doubleconversion,concurrent]
cd ..
cmake --preset ninja-vcpkg
cmake --build --preset ninja-vcpkg-release --target clap-host
