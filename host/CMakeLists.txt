set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_executable(clap-host
  about-dialog.hh
  about-dialog.cc
  application.cc
  application.hh
  audio-settings.cc
  audio-settings.hh
  audio-settings-widget.cc
  audio-settings-widget.hh

  plugin-param.cc
  plugin-param.hh
  plugin-host.cc
  plugin-host.hh
  plugin-quick-control-widget.cc
  plugin-quick-control-widget.hh
  plugin-quick-controls-widget.cc
  plugin-quick-controls-widget.hh

  CMakeLists.txt
  device-reference.hh
  engine.cc
  engine.hh
  main.cc
  main-window.cc
  main-window.hh
  midi-settings.cc
  midi-settings.hh
  midi-settings-widget.cc
  midi-settings-widget.hh
  plugin-info.hh
  plugin-parameters-widget.cc
  plugin-parameters-widget.hh
  plugin-host-settings.cc
  plugin-host-settings.hh
  settings.cc
  settings-dialog.cc
  settings-dialog.hh
  settings.hh
  settings-widget.cc
  settings-widget.hh
  tweaks-dialog.cc
  tweaks-dialog.hh

  precompiled-header.hh
  )

target_link_libraries(clap-host PRIVATE Qt6::Widgets clap-helpers)
target_precompile_headers(clap-host PRIVATE precompiled-header.hh)

if(UsePkgConfig)
  target_link_libraries(clap-host PRIVATE PkgConfig::RtMidi PkgConfig::RtAudio)
else()
  target_link_libraries(clap-host PRIVATE RtAudio::rtaudio RtMidi::rtmidi)
endif()

if (LINUX)
  target_link_libraries(clap-host PRIVATE dl pthread)
endif()

if (APPLE)
  # Unclear why this was here, but it breaks ARM builds.
  # set_target_properties(clap-host PROPERTIES OSX_ARCHITECTURES x86_64)

  find_library(CORE_FOUNDATION CoreFoundation)
  find_library(CORE_AUDIO CoreAudio)
  find_library(CORE_AUDIO CoreServices)
  find_library(CORE_MIDI CoreMIDI)
  find_library(AUDIO_UNIT AudioUnit)
  find_library(AUDIO_TOOLBOX AudioToolbox)
  find_library(CARBON Carbon)
  target_link_libraries(clap-host PRIVATE ${CARBON} ${AUDIO_UNIT} ${AUDIO_TOOLBOX} ${CORE_MIDI} ${CORE_AUDIO} ${CORE_SERVICES} ${CORE_FOUNDATION})
  set_target_properties(clap-host PROPERTIES MACOSX_BUNDLE CLAP_HOST_BUNDLE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(clap-host PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linux-clap-host.version)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_options(clap-host PRIVATE -exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/macos-symbols.txt)
endif()

if(NOT ${CLAP_HOST_BINARY} STREQUAL clap-host)
  set_target_properties(clap-host PROPERTIES OUTPUT_NAME ${CLAP_HOST_BINARY})
endif()

install(TARGETS clap-host DESTINATION "bin" COMPONENT host)
