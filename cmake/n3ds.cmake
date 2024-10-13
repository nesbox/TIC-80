################################
# TIC-80 app (N3DS)
################################

if(NINTENDO_3DS)
    set(TIC80_SRC ${TIC80_SRC}
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/utils.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/main.c
    )

    add_executable(tic80 ${TIC80_SRC})

    target_include_directories(tic80 PRIVATE
        ${DEVKITPRO}/portlibs/3ds/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src)

    target_link_directories(tic80 PRIVATE ${DEVKITPRO}/libctru/lib ${DEVKITPRO}/portlibs/3ds/lib)
    target_link_libraries(tic80 tic80studio png citro3d)

    ctr_generate_smdh(tic80.smdh
        NAME        "TIC-80 tiny computer"
        DESCRIPTION "Fantasy computer for making, playing and sharing tiny games"
        AUTHOR      "Nesbox"
        ICON        ${CMAKE_SOURCE_DIR}/build/n3ds/icon.png
    )

    ctr_create_3dsx(tic80
        SMDH   tic80.smdh
        ROMFS  ${CMAKE_SOURCE_DIR}/build/n3ds/romfs
        OUTPUT ${CMAKE_SOURCE_DIR}/build/bin/tic80.3dsx
    )

endif()