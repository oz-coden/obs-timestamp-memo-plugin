cmake_minimum_required(VERSION 3.28)
project(timestamp_memo_regressions LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options($<$<CXX_COMPILER_ID:Clang,AppleClang>:-Werror=range-loop-bind-reference>)
enable_testing()
add_executable(
  domain_tests
  domain-tests.cpp
  ../src/core/timecode-helper.cpp
  ../src/core/memo-marker.cpp
  ../src/core/recording-session.cpp
  ../src/exporters/text-formats.cpp
)
target_include_directories(domain_tests PRIVATE ../src/core ../src/exporters)
if(MSVC)
  target_compile_options(domain_tests PRIVATE /W4 /WX /EHsc /utf-8)
else()
  target_compile_options(domain_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME domain COMMAND domain_tests)

option(ENABLE_INTEGRATION_TESTS "Build Qt/OBS adapter integration tests" ON)
if(NOT ENABLE_INTEGRATION_TESTS)
  return()
endif()
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

file(GLOB application_sources "../src/application/*.cpp" "../src/application/*.hpp")
file(GLOB persistence_sources "../src/persistence/*.cpp" "../src/persistence/*.hpp")
file(GLOB core_sources "../src/core/*.cpp" "../src/core/*.hpp")
file(GLOB exporter_sources "../src/exporters/*.cpp" "../src/exporters/*.hpp")
file(GLOB ui_sources "../src/ui/*.cpp" "../src/ui/*.hpp")
file(GLOB bridge_sources "../src/obs/*.cpp" "../src/obs/*.hpp")
add_executable(
  regressions
  regressions.cpp
  obs-stubs.cpp
  ../src/plugin-main.cpp
  ${application_sources}
  ${persistence_sources}
  ${core_sources}
  ${exporter_sources}
  ${ui_sources}
  ${bridge_sources}
)
target_include_directories(
  regressions
  PRIVATE stubs ../src ../src/core ../src/persistence ../src/application ../src/exporters ../src/ui ../src/obs
)
target_link_libraries(regressions PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(regressions PRIVATE TEST_LOCALE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../data/locale")
if(MSVC)
  target_compile_definitions(regressions PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(regressions PRIVATE /W4 /WX /EHsc /utf-8)
else()
  target_compile_options(regressions PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME regressions COMMAND regressions)
set_tests_properties(regressions PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal" TIMEOUT 30)
if(WIN32)
  set_tests_properties(
    regressions
    PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()
add_executable(
  application_tests
  application-tests.cpp
  ../src/application/session-controller.cpp
  ../src/application/session-controller.hpp
  ../src/application/recording-gateway.hpp
  ${core_sources}
  ${exporter_sources}
  ${persistence_sources}
)
target_include_directories(application_tests PRIVATE ../src/application ../src/core ../src/exporters ../src/persistence)
target_link_libraries(application_tests PRIVATE Qt6::Core Qt6::Gui)
if(MSVC)
  target_compile_options(application_tests PRIVATE /W4 /WX /EHsc /utf-8)
else()
  target_compile_options(application_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME application COMMAND application_tests)
if(WIN32)
  set_tests_properties(
    application
    PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
endif()
