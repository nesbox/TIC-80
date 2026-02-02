################################
# TIC-80 app (Switch)
################################

if(NINTENDO_SWITCH)
    if(BUILD_EDITORS)
        target_sources(tic80studio PRIVATE
            src/system/nswitch/net.c)
        target_include_directories(tic80studio
            PRIVATE ${TIC80LIB_DIR}/studio)
    endif()

    target_sources(${TIC80_TARGET} PRIVATE
        src/system/nswitch/runtime.c)

    find_package(PkgConfig)
    pkg_search_module(CURL libcurl IMPORTED_TARGET)
    target_link_libraries(tic80 PkgConfig::CURL)

    nx_generate_nacp(tic80.nacp
        NAME    "TIC-80 tiny computer"
        AUTHOR  "Nesbox, carstene1ns"
        VERSION ${PROJECT_VERSION})

    nx_create_nro(${TIC80_TARGET}
        NACP   tic80.nacp
        ICON   ${CMAKE_SOURCE_DIR}/build/nswitch/icon.jpg
        #ROMFS  ${CMAKE_SOURCE_DIR}/build/nswitch/romfs
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tic80.nro)

    dkp_target_generate_symbol_list(${TIC80_TARGET})
endif()
