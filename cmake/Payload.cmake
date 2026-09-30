# Packs the source archives (CLEO, SAMPFUNCS, MoonLoader, ASI loader, script, fonts)
# into payload.zip + payload_manifest.gen.h. The archives are NOT stored in git.

set(UF_PAYLOAD_SRC "$ENV{USERPROFILE}/Desktop/uf-installer" CACHE PATH "Folder with the source archives")
file(TO_CMAKE_PATH "${UF_PAYLOAD_SRC}" UF_PAYLOAD_SRC)
if(NOT IS_DIRECTORY "${UF_PAYLOAD_SRC}")
    message(FATAL_ERROR
        "Payload folder not found: ${UF_PAYLOAD_SRC}\n"
        "Put the archives (CLEO.zip, SAMPFUNCS_*.zip, moonloader_*.zip, silents_asi_loader_*.zip, "
        "UltraFuck_*.zip, fonts .rar/.zip) there or pass -DUF_PAYLOAD_SRC=<folder>.")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(GLOB UF_PAYLOAD_INPUTS CONFIGURE_DEPENDS "${UF_PAYLOAD_SRC}/*.zip" "${UF_PAYLOAD_SRC}/*.rar")
set(UF_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(UF_PAYLOAD_ZIP "${UF_GEN_DIR}/payload.zip")
set(UF_MANIFEST_H "${UF_GEN_DIR}/payload_manifest.gen.h")

add_custom_command(
    OUTPUT "${UF_PAYLOAD_ZIP}" "${UF_MANIFEST_H}"
    COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/pack_payload.py"
            --src "${UF_PAYLOAD_SRC}" --out "${UF_GEN_DIR}" --cmake "${CMAKE_COMMAND}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/pack_payload.py" ${UF_PAYLOAD_INPUTS}
    COMMENT "Packing installer payload from ${UF_PAYLOAD_SRC}"
    VERBATIM)
add_custom_target(uf_payload DEPENDS "${UF_PAYLOAD_ZIP}" "${UF_MANIFEST_H}")

# Resource script with absolute paths to the embedded files.
set(UF_RES_DIR "${CMAKE_SOURCE_DIR}/res")
configure_file("${UF_RES_DIR}/resources.rc.in" "${UF_GEN_DIR}/resources.rc" @ONLY)
set_source_files_properties("${UF_GEN_DIR}/resources.rc" PROPERTIES
    OBJECT_DEPENDS "${UF_PAYLOAD_ZIP};${UF_RES_DIR}/app.ico;${UF_RES_DIR}/resource.h;${UF_RES_DIR}/fonts/IBMPlexSans-Regular.ttf;${UF_RES_DIR}/fonts/IBMPlexSans-SemiBold.ttf;${UF_RES_DIR}/fonts/IBMPlexMono-Regular.ttf;${UF_RES_DIR}/fonts/Phosphor.ttf;${UF_RES_DIR}/fonts/Phosphor-Bold.ttf")
