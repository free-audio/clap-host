# https://vcpkg.io/en/docs/users/triplets.html

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_C_FLAGS_RELEASE "-Os -fomit-frame-pointer")
set(VCPKG_CXX_FLAGS_RELEASE "-Os -fomit-frame-pointer")

if(NOT PORT MATCHES "harfbuzz")
    # harfbuzz is badly packaged for now, so we also make the debug build
    # https://github.com/microsoft/vcpkg/issues/22680
    set(VCPKG_BUILD_TYPE release)
endif()

if(PORT MATCHES "alsa")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

set(QT_NAMESPACE ClapHost)
