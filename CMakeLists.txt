cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0091 NEW) # use CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0100 NEW) # handle .hh files

enable_testing()

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CLAP_ENABLE_HOST TRUE CACHE BOOL "Enable example host")
set(CLAP_ENABLE_PLUGINS TRUE CACHE BOOL "Enable example plugins")

project(CLAP_EXAMPLES C CXX)

set(UsePkgConfig TRUE CACHE BOOL "Use PkgConfig to find RtMidi and RtAudio dependencies")

set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "AppleClang|Clang|GNU")
  add_compile_options(-fvisibility=hidden -fvisibility-inlines-hidden
    -Werror=return-type -Werror=unused-result -Wno-unused-parameter
    -Wuninitialized -Wvla -Wmultichar -Werror=non-virtual-dtor -Woverloaded-virtual)
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "AppleClang|Clang")
  add_compile_options(-Wmost -Wsuper-class-method-mismatch)
endif()

add_subdirectory(clap EXCLUDE_FROM_ALL)
add_subdirectory(clap-helpers EXCLUDE_FROM_ALL)

if(CLAP_ENABLE_HOST)
  if(UsePkgConfig)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RtMidi REQUIRED IMPORTED_TARGET rtmidi)
    pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
  else()
    find_package(RtMidi CONFIG REQUIRED)
    find_package(RtAudio CONFIG REQUIRED)
  endif()

  add_subdirectory(host)
endif()

if(CLAP_ENABLE_PLUGINS AND FALSE)
  find_package(Boost COMPONENTS serialization iostreams REQUIRED)
  add_subdirectory(plugins)
endif()

###########
## CPACK ##
###########

set(CPACK_PACKAGE_NAME "clap-examples")
set(CPACK_PACKAGE_VENDOR "CLAP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CLAP Reference Plugins and Host")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/free-audio/clap-examples")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "clap-examples")
set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
set(CPACK_NSIS_plugins_INSTALL_DIRECTORY "$COMMONPROGRAMDATA/CLAP")

include(CPack)

if(CLAP_ENABLE_HOST AND CLAP_ENABLE_PLUGINS)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_custom_target(run-host
      COMMAND $<TARGET_FILE:clap-host> -p $<TARGET_BUNDLE_DIR:clap-plugins> -i 0
      DEPENDS clap-host clap-plugins
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIRECTORY}")
  else()
    add_custom_target(run-host
      COMMAND $<TARGET_FILE:clap-host> -p $<TARGET_FILE:clap-plugins> -i 0
      DEPENDS clap-host clap-plugins
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIRECTORY}")
  endif()
endif()