################################
# TIC-80 app (N3DS)
################################

if(N3DS)
    set(TIC80_SRC ${TIC80_SRC}
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/utils.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/main.c
    )

    add_executable(tic80_n3ds ${TIC80_SRC})

    target_include_directories(tic80_n3ds PRIVATE
        ${DEVKITPRO}/portlibs/3ds/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src)

    target_link_directories(tic80_n3ds PRIVATE ${DEVKITPRO}/libctru/lib ${DEVKITPRO}/portlibs/3ds/lib)
    target_link_libraries(tic80_n3ds tic80studio png citro3d)

    add_custom_command(TARGET tic80_n3ds
           POST_BUILD
               COMMAND ${CMAKE_SOURCE_DIR}/build/n3ds/elf_to_3dsx.sh
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build
    )
endif()