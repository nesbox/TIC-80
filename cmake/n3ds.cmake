################################
# TIC-80 app (N3DS)
################################

if(NINTENDO_3DS)
    if(BUILD_EDITORS)
        target_sources(tic80studio PRIVATE
            src/system/n3ds/net.c)
        target_include_directories(tic80studio
            PRIVATE ${TIC80LIB_DIR}/studio)
    endif()

    add_executable(${TIC80_TARGET} ${TIC80_SRC}
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/utils.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/main.c)

    target_link_libraries(${TIC80_TARGET} tic80studio png citro3d)

    ctr_generate_smdh(tic80.smdh
        NAME        "TIC-80 tiny computer"
        DESCRIPTION "Fantasy computer for making, playing and sharing tiny games"
        AUTHOR      "Nesbox, asie"
        ICON        ${CMAKE_SOURCE_DIR}/build/n3ds/icon.png)

    ctr_create_3dsx(${TIC80_TARGET}
        SMDH   tic80.smdh
        ROMFS  ${CMAKE_SOURCE_DIR}/build/n3ds/romfs
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tic80.3dsx)
endif()
