# Third-party sources, pinned by URL + SHA256.
# Downloads are cached in ${UF_DEPS_CACHE} (outside the build dir) so clean rebuilds work offline.

set(UF_DEPS_CACHE "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Download cache for third-party sources")
set(UF_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
set(UF_DEPS_INCLUDE "${UF_DEPS_DIR}/include")

function(uf_download name url sha256 out_var)
    set(dst "${UF_DEPS_CACHE}/${name}")
    if(EXISTS "${dst}")
        file(SHA256 "${dst}" have)
        if(NOT have STREQUAL sha256)
            message(STATUS "Cached ${name} has a wrong hash, downloading again")
            file(REMOVE "${dst}")
        endif()
    endif()
    if(NOT EXISTS "${dst}")
        message(STATUS "Downloading ${name}")
        file(DOWNLOAD "${url}" "${dst}.part" EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS st)
        list(GET st 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${dst}.part")
            list(GET st 1 msg)
            message(FATAL_ERROR "Failed to download ${url}: ${msg}")
        endif()
        file(RENAME "${dst}.part" "${dst}")
    endif()
    set(${out_var} "${dst}" PARENT_SCOPE)
endfunction()

function(uf_extract archive dest)
    if(NOT EXISTS "${dest}/.extracted")
        file(REMOVE_RECURSE "${dest}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dest}")
        file(TOUCH "${dest}/.extracted")
    endif()
endfunction()

# Dear ImGui
uf_download(imgui-1.92.9b.zip
    "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.zip"
    e1c46d676c2bcb7ced847ba27f50553e33a19db97b3cadaec7f8be64449139f8 imgui_zip)
uf_extract("${imgui_zip}" "${UF_DEPS_DIR}/imgui")
set(IMGUI_DIR "${UF_DEPS_DIR}/imgui/imgui-1.92.9b")

# miniz (amalgamated release)
uf_download(miniz-3.1.2.zip
    "https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip"
    f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a miniz_zip)
uf_extract("${miniz_zip}" "${UF_DEPS_DIR}/miniz")
set(MINIZ_DIR "${UF_DEPS_DIR}/miniz")

# LZMA SDK (public domain): only LzmaDec.c, for the LZMA entries of payload.zip (miniz reads deflate only)
uf_download(lzma2501.7z
    "https://github.com/ip7z/7zip/releases/download/25.01/lzma2501.7z"
    cbc3babd589d971e45971d787ff100be8aaa5eab15b2694497ec3e447009e1f2 lzma_7z)
uf_extract("${lzma_7z}" "${UF_DEPS_DIR}/lzma")
set(LZMA_DIR "${UF_DEPS_DIR}/lzma/C")

# nlohmann/json (single header)
uf_download(json-3.12.0.hpp
    "https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp"
    aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63 json_hpp)
configure_file("${json_hpp}" "${UF_DEPS_INCLUDE}/nlohmann/json.hpp" COPYONLY)

# doctest (single header)
uf_download(doctest-2.4.12.h
    "https://raw.githubusercontent.com/doctest/doctest/v2.4.12/doctest/doctest.h"
    94029a7d32da24a56249658147dbd2b33ff0b9ed665295cbbaf19aafff5b0ced doctest_h)
configure_file("${doctest_h}" "${UF_DEPS_INCLUDE}/doctest/doctest.h" COPYONLY)

# nanosvg (SVG parser + rasterizer, headers only): the language flags
set(NANOSVG_COMMIT 239e102ec2c691f2902e20ace2ed36ee4a35cfe6)
uf_download(nanosvg-${NANOSVG_COMMIT}.h
    "https://raw.githubusercontent.com/memononen/nanosvg/${NANOSVG_COMMIT}/src/nanosvg.h"
    e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0 nanosvg_h)
uf_download(nanosvgrast-${NANOSVG_COMMIT}.h
    "https://raw.githubusercontent.com/memononen/nanosvg/${NANOSVG_COMMIT}/src/nanosvgrast.h"
    79a9c5f4db19debf9f3a648a1589e96d92854f245a5cb4f3d823f263785234d8 nanosvgrast_h)
configure_file("${nanosvg_h}" "${UF_DEPS_INCLUDE}/nanosvg/nanosvg.h" COPYONLY)
configure_file("${nanosvgrast_h}" "${UF_DEPS_INCLUDE}/nanosvg/nanosvgrast.h" COPYONLY)

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx9.cpp")
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_compile_definitions(imgui PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    IMGUI_USE_BGRA_PACKED_COLOR)
target_compile_options(imgui PRIVATE /W0)

add_library(miniz STATIC "${MINIZ_DIR}/miniz.c")
target_include_directories(miniz PUBLIC "${MINIZ_DIR}")
target_compile_options(miniz PRIVATE /W0)

add_library(lzmadec STATIC "${LZMA_DIR}/LzmaDec.c")
target_include_directories(lzmadec PUBLIC "${LZMA_DIR}")
target_compile_options(lzmadec PRIVATE /W0)
