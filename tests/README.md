# Regression tests

These tests compile the production C++ and Qt UI against small OBS API stubs.
They do not load OBS or change its configuration. QSettings and session files use
a temporary directory. An actual OBS run is still required for encoder timing,
hotkey persistence, dock placement, and NLE interoperability.

Requirements: CMake 3.28+, a C++20 compiler, and Qt6 Core/Gui/Widgets with the
minimal platform plugin. The bundled Windows Qt package has minimal but does not
have offscreen; CTest selects minimal and adds the Qt DLL directory to PATH.

Windows (with the dependencies downloaded by the plugin build):

    cmake -S tests -B build_tests -A x64 -DCMAKE_PREFIX_PATH="$PWD/.deps/obs-deps-qt6-2025-07-11-x64"
    cmake --build build_tests --config RelWithDebInfo --parallel
    ctest --test-dir build_tests -C RelWithDebInfo --output-on-failure

Linux/macOS (set CMAKE_PREFIX_PATH if Qt is outside its default search paths):

    cmake -S tests -B build_tests -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build_tests --parallel
    ctest --test-dir build_tests --output-on-failure

The executable contains six groups: timecodes; JSON/FPS/edit persistence;
journal recovery and failures; eight exporters and dialog snapshots; model
reentrancy; and module/controller/UI lifecycle. The final group exercises
worker-thread split events, a split immediately before stop, pause stamping,
encoder FPS divisors/scaling, context-menu reentrancy, repeated initialization,
EXIT cleanup, and failed dock registration.

Warnings are errors (/W4 /WX on MSVC, -Wall -Wextra -Werror otherwise).
OBS stubs reproduce the reviewed OBS 31.1.1 interfaces and callback ordering;
they cannot establish binary compatibility or actual muxer timing.

For clang-tidy against the production compile database, Windows clang needs the
declaration-only analyzer-compat.hpp for OBS 31.1.1's _udiv128 header path:

    clang-tidy src/obs/obs-bridge.cpp -p . --checks=-*,clang-analyzer-core.*,clang-analyzer-cplusplus.*,clang-analyzer-deadcode.* --extra-arg=-include --extra-arg=tests/analyzer-compat.hpp

That compatibility header is only for analysis and is not linked into the plugin.
See [OBS manual tests](../docs/obs-manual-test.md) for remaining runtime checks.
