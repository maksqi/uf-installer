cmake_minimum_required(VERSION 3.25)
project(uf_installer VERSION 1.0.0 LANGUAGES C CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "Build with MSVC (use build.ps1, it enters the VS x86 developer shell).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "The installer is meant to be built for x86 (it must see the same d3dx9 as gta_sa.exe).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_compile_definitions(
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0601 WINVER=0x0601
    _CRT_SECURE_NO_WARNINGS)
add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /EHsc /bigobj)
# Our app.manifest carries the trustInfo (asInvoker) itself.
add_link_options(/MANIFESTUAC:NO)
add_link_options($<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)

include(cmake/Deps.cmake)
include(cmake/Payload.cmake)

# ---------------------------------------------------------------- core
add_library(uf_core STATIC
    src/core/args.cpp
    src/core/arizona.cpp
    src/core/directx.cpp
    src/core/discovery.cpp
    src/core/elevation.cpp
    src/core/fonts.cpp
    src/core/fsutil.cpp
    src/core/i18n.cpp
    src/core/i18n_uk.cpp
    src/core/inspect.cpp
    src/core/installer.cpp
    src/core/known.cpp
    src/core/log.cpp
    src/core/paths.cpp
    src/core/payload.cpp
    src/core/pe.cpp
    src/core/plan.cpp
    src/core/registry.cpp
    src/core/versions.cpp
    "${UF_MANIFEST_H}")
target_include_directories(uf_core PUBLIC src "${UF_GEN_DIR}" "${UF_DEPS_INCLUDE}" "${UF_RES_DIR}")
target_link_libraries(uf_core PUBLIC miniz lzmadec version shlwapi ole32 shell32 advapi32 user32 gdi32 urlmon wintrust crypt32 mpr)
target_compile_options(uf_core PRIVATE /W4)
add_dependencies(uf_core uf_payload)

set(UF_RESOURCES "${UF_GEN_DIR}/resources.rc" "${UF_RES_DIR}/app.manifest")

# ---------------------------------------------------------------- GUI
add_executable(uf-installer WIN32
    src/ui/main_win.cpp
    src/ui/app.cpp
    src/ui/d3d9_device.cpp
    src/ui/svg.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    src/ui/screens/common.cpp
    src/ui/screens/select.cpp
    src/ui/screens/analysis.cpp
    src/ui/screens/progress.cpp
    ${UF_RESOURCES})
target_include_directories(uf-installer PRIVATE src)
target_link_libraries(uf-installer PRIVATE uf_core imgui d3d9 dwmapi imm32 windowscodecs comctl32)
target_compile_options(uf-installer PRIVATE /W4)

# ---------------------------------------------------------------- tests
enable_testing()
add_executable(uf-tests tests/test_main.cpp tests/test_core.cpp tests/test_plan.cpp tests/test_install.cpp ${UF_RESOURCES})
target_link_libraries(uf-tests PRIVATE uf_core)
add_test(NAME uf-tests COMMAND uf-tests)
