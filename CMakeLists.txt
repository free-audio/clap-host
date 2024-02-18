cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0091 NEW) # use CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0100 NEW) # handle .hh files
cmake_policy(SET CMP0110 NEW) # arbitrary test name

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

project(CLAP_EXAMPLES C CXX)

set(UsePkgConfig TRUE CACHE BOOL "Use PkgConfig to find RtMidi and RtAudio dependencies")
set(CLAP_HOST_BUNDLE FALSE CACHE BOOL "Produce a macOS bundle")
set(CLAP_HOST_BINARY clap-host CACHE STRING "File name of the resulting binary")

include(cmake/Dependencies.cmake)

set(CMAKE_AUTOMOC ON)


if(${CMAKE_CXX_COMPILER_ID} MATCHES "AppleClang|Clang|GNU")
  add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden
    -Werror=return-type -Werror=unused-result -Wno-unused-parameter
    -Wuninitialized -Wvla -Wmultichar -Werror=non-virtual-dtor -Woverloaded-virtual)
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "AppleClang|Clang")
  add_compile_options(-Wmost -Wsuper-class-method-mismatch)
endif()

#find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(Qt6Core CONFIG REQUIRED)
find_package(Qt6Widgets CONFIG REQUIRED)
if(UsePkgConfig)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RtMidi REQUIRED IMPORTED_TARGET rtmidi)
  pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
else()
  find_package(RtMidi CONFIG REQUIRED)
  find_package(RtAudio CONFIG REQUIRED)
endif()
find_package(clap CONFIG REQUIRED)
find_package(clap-helpers CONFIG REQUIRED)

add_subdirectory(host)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  configure_file(resources/linux/org.cleveraudio.clap-host.desktop.in resources/linux/org.cleveraudio.clap-host.desktop)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/resources/linux/org.cleveraudio.clap-host.desktop DESTINATION share/applications)
  install(FILES resources/linux/org.cleveraudio.clap-host.appdata.xml DESTINATION share/metainfo)
  install(FILES "clap/artwork/CLAP Full Logo.svg" DESTINATION share/icons/hicolor/scalable/apps RENAME org.cleveraudio.clap-host.svg)
endif()

###########
## CPACK ##
###########

set(CPACK_PACKAGE_NAME "clap-host")
set(CPACK_PACKAGE_VENDOR "CLAP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CLAP Reference Host")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/free-audio/clap-host")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "clap-host")
set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)

include(CPack)
