# Tests

Three layers test behavior without launching OBS:

|Target|Dependencies|Coverage|
|---|---|---|
|domain_tests|C++20 only|FPS/timecode, exact marker frames, document CRUD/IDs, timeline transitions and segments, shared export text|
|application_tests|Qt6 Core/Gui, FakeGateway|Recording commands, delayed path, split rejection, edits/history, serialization/recovery, settings conversion and failed writes, repeated initialization|
|regressions|Qt6 Core/Gui/Widgets, OBS stubs|All eight exporters, dialog snapshots, clipboard, model edits/reentrancy, worker events, hotkeys, registration/EXIT/unload|

Tests use temporary session files and explicitly select INI QSettings in a
temporary directory. They do not load OBS or write its configuration. The
production configuration now uses an explicit INI in the OBS module config
directory, with read-only legacy migration. Tests use separate temporary config
roots and never modify an actual OBS installation.

Requirements: CMake 3.28+, C++20 compiler, and (for the full suite) Qt6 with the
minimal platform plugin. CTest selects minimal and adds the Qt DLL directory to
PATH. For the reported standalone regressions GUI platform initialization error,
run through CTest with this environment. The exact cause of that earlier error
has not been reproduced.

Windows, after the plugin dependencies have been downloaded:

    cmake -S tests -B build_tests -A x64 -DCMAKE_PREFIX_PATH="$PWD/.deps/obs-deps-qt6-2025-07-11-x64"
    cmake --build build_tests --config RelWithDebInfo --parallel
    ctest --test-dir build_tests -C RelWithDebInfo --output-on-failure

Linux/macOS (set CMAKE_PREFIX_PATH when necessary):

    cmake -S tests -B build_tests -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build_tests --parallel
    ctest --test-dir build_tests --output-on-failure

Pure logic alone, without installing Qt or OBS:

    cmake -S tests -B build_domain -DENABLE_INTEGRATION_TESTS=OFF
    cmake --build build_domain --config RelWithDebInfo --parallel
    ctest --test-dir build_domain -C RelWithDebInfo --output-on-failure

Warnings are errors (/W4 /WX on MSVC, -Wall -Wextra -Werror otherwise).
Clipboard success is checked without a timer dismissing modal dialogs, so a
reintroduced success modal fails by test timeout. Model edits verify propagation
through the controller instead of duplicating state changes in the widget.
The unload stub deliberately defers outer dock deletion to exercise service/view
lifetime ordering. OBS stubs reproduce the reviewed OBS 31.1.1 APIs; actual muxer
timing, dock placement, hotkey persistence and NLE interoperability need OBS.

For clang-tidy, obtain a compile database from the production build (e.g. a
Ninja build with CMAKE_EXPORT_COMPILE_COMMANDS=ON). Windows clang also needs the
declaration-only analyzer-compat.hpp for OBS 31.1.1's _udiv128 header path:

    clang-tidy src/obs/obs-bridge.cpp -p BUILD_WITH_COMPILE_DATABASE --checks=-*,clang-analyzer-core.*,clang-analyzer-cplusplus.*,clang-analyzer-deadcode.* --extra-arg=-include --extra-arg=tests/analyzer-compat.hpp

That compatibility header is never linked into the plugin. The local Windows
review used an ignored analysis database reflecting all current source paths and
include directories. All 26 implementation files were analyzed; a Qt-owned
layout produces a known potential leak false positive, with ownership checked by
the dialog lifetime regressions.

See the [changelog](../CHANGELOG.md) for user-facing changes and limitations.

Regression coverage includes rendered-video/packet-PTS calibration, delayed/B-frame
anchors, pause/resume, fractional FPS, Undo/Redo limits and persistence, unsaved
discard, collision-safe auto exports, plugin config and global hotkey migration,
notification severity, Markdown privacy/escaping, CSV text/formula protection,
auto JSON OFF edits, detailed transactional validation and actual locale tests.
Real encoder/muxer accuracy and native keyboard/clipboard behavior still need the
actual OBS validation.
