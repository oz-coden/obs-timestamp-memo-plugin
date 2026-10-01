cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/EHsc>)

if(NOT ENABLE_FRONTEND_API OR NOT ENABLE_QT)
  message(FATAL_ERROR "Timestamp Memo requires ENABLE_FRONTEND_API=ON and ENABLE_QT=ON")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF
)

target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/src/core
          ${CMAKE_CURRENT_SOURCE_DIR}/src/persistence
          ${CMAKE_CURRENT_SOURCE_DIR}/src/exporters
          ${CMAKE_CURRENT_SOURCE_DIR}/src/obs
          ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(TARGET plugin-support)
  target_link_libraries(plugin-support PRIVATE OBS::libobs)
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/core/recording-timeline.hpp
          src/persistence/session-codec.hpp
          src/persistence/session-codec.cpp
          src/persistence/session-store.hpp
          src/persistence/session-store.cpp
          src/exporters/text-formats.hpp
          src/exporters/text-formats.cpp
          src/plugin-main.cpp
          src/plugin-support.h
          src/core/timecode-helper.hpp
          src/core/timecode-helper.cpp
          src/core/memo-marker.hpp
          src/core/memo-marker.cpp
          src/core/plugin-config.hpp
          src/core/plugin-config.cpp
          src/core/recording-session.hpp
          src/core/recording-session.cpp
          src/core/session-controller.hpp
          src/core/session-controller.cpp
          src/exporters/exporter-base.hpp
          src/exporters/exporter-registry.hpp
          src/exporters/exporter-registry.cpp
          src/exporters/json-exporter.hpp
          src/exporters/json-exporter.cpp
          src/exporters/csv-exporter.hpp
          src/exporters/csv-exporter.cpp
          src/exporters/edl-exporter.hpp
          src/exporters/edl-exporter.cpp
          src/exporters/srt-exporter.hpp
          src/exporters/srt-exporter.cpp
          src/exporters/vtt-exporter.hpp
          src/exporters/vtt-exporter.cpp
          src/exporters/youtube-exporter.hpp
          src/exporters/youtube-exporter.cpp
          src/exporters/markdown-exporter.hpp
          src/exporters/markdown-exporter.cpp
          src/exporters/xml-exporter.hpp
          src/exporters/xml-exporter.cpp
          src/obs/obs-bridge.hpp
          src/obs/obs-bridge.cpp
          src/obs/status-notifier.hpp
          src/obs/status-notifier.cpp
          src/ui/marker-table-model.hpp
          src/ui/marker-table-model.cpp
          src/ui/dock-widget.hpp
          src/ui/dock-widget.cpp
          src/ui/settings-dialog.hpp
          src/ui/settings-dialog.cpp
          src/ui/export-dialog.hpp
          src/ui/export-dialog.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
