################################
# TIC-80 app (Symbian)
################################

if(SYMBIAN)
    set(TIC80_SRC
        ${TIC80_SRC}
        ${CMAKE_SOURCE_DIR}/src/system/symbian/app.cpp
        ${CMAKE_SOURCE_DIR}/src/system/symbian/document.cpp
        ${CMAKE_SOURCE_DIR}/src/system/symbian/appui.cpp
        ${CMAKE_SOURCE_DIR}/src/system/symbian/container.cpp
        ${CMAKE_SOURCE_DIR}/src/system/symbian/tic.cpp
        ${CMAKE_SOURCE_DIR}/src/system/symbian/mapping.c  # TODO:XXX: Fixed mapping for Nokia E7; use IME if possible?
    )

    add_executable(tic80 ${TIC80_SRC})
    target_link_libraries(tic80
        :libstdcpp.dso
    )
    target_compile_definitions(tic80 PRIVATE SYMBIAN_FIXED_KEYMAP)
    target_compile_definitions(tic80 PRIVATE USE_GLES2)
    target_link_libraries(tic80
        :libGLESv2.dso
        :libegl.dso
    )

    set_property(TARGET tic80 PROPERTY POSITION_INDEPENDENT_CODE FALSE)
    set_property(TARGET tic80 PROPERTY COMPILE_FLAGS "-O1")
    set_target_properties(tic80 PROPERTIES PREFIX "")

    target_include_directories(tic80 PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(tic80 tic80studio png)

    elf2e32(tic80 ${CMAKE_CURRENT_BINARY_DIR}/bin/tic80 0xE1C57D10)
    rcomp(tic80 ${CMAKE_CURRENT_SOURCE_DIR}/build/symbian/tic80)
    rcomp(tic80_reg ${CMAKE_CURRENT_SOURCE_DIR}/build/symbian/tic80_reg)
    add_dependencies(tic80_reg-rsc
        tic80-rsc
    )
    mifconv(tic80_icon tic80_icon.svg)
    add_custom_target(tic80_icon-svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/build/symbian/tic80_icon.svg ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_dependencies(tic80_icon-mif
        tic80_icon-svg
    )
    makesis(tic80 ${CMAKE_CURRENT_SOURCE_DIR}/build/symbian/tic80.pkg)
    add_dependencies(tic80-sis
        tic80-e32
        tic80-rsc
        tic80_reg-rsc
        tic80_icon-mif
    )
endif()