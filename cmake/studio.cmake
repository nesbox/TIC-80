################################
# TIC-80 studio
################################

set(TIC80LIB_DIR ${CMAKE_SOURCE_DIR}/src)
set(TIC80STUDIO_SRC
    ${TIC80LIB_DIR}/studio/screens/run.c
    ${TIC80LIB_DIR}/studio/screens/menu.c
    ${TIC80LIB_DIR}/studio/screens/mainmenu.c
    ${TIC80LIB_DIR}/studio/screens/start.c
    ${TIC80LIB_DIR}/studio/studio.c
    ${TIC80LIB_DIR}/studio/config.c
    ${TIC80LIB_DIR}/studio/fs.c
    ${TIC80LIB_DIR}/ext/md5.c
    ${TIC80LIB_DIR}/ext/json.c
)

if(BUILD_EDITORS)
    set(TIC80STUDIO_SRC ${TIC80STUDIO_SRC}
        ${TIC80LIB_DIR}/studio/screens/console.c
        ${TIC80LIB_DIR}/studio/screens/surf.c
        ${TIC80LIB_DIR}/studio/editors/code.c
        ${TIC80LIB_DIR}/studio/editors/sprite.c
        ${TIC80LIB_DIR}/studio/editors/map.c
        ${TIC80LIB_DIR}/studio/editors/world.c
        ${TIC80LIB_DIR}/studio/editors/sfx.c
        ${TIC80LIB_DIR}/studio/editors/music.c
        ${TIC80LIB_DIR}/studio/net.c
        ${TIC80LIB_DIR}/ext/history.c
        ${TIC80LIB_DIR}/ext/gif.c
        ${TIC80LIB_DIR}/ext/png.c
    )
endif()

if(BUILD_PRO)
    set(TIC80STUDIO_SRC ${TIC80STUDIO_SRC}
        ${TIC80LIB_DIR}/studio/project.c)
endif()

set(TIC80_OUTPUT tic80)

# Generate embedded HTML templates (optional feature)
option(BUILD_HTMLLOCAL_EXPORT "Enable htmllocal export with embedded templates" OFF)

if(BUILD_HTMLLOCAL_EXPORT)
    # Check if web build artifacts exist (they should be pre-built)
    if(EXISTS ${CMAKE_SOURCE_DIR}/build/html/export.html AND EXISTS ${CMAKE_SOURCE_DIR}/build/bin/tic80.js AND EXISTS ${CMAKE_SOURCE_DIR}/build/bin/tic80.wasm)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_html_templates.h
            COMMAND ${CMAKE_COMMAND} -E echo "Generating embedded HTML templates..."
            COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/embed_html.py ${CMAKE_CURRENT_BINARY_DIR}/embedded_html_templates.h
            DEPENDS ${CMAKE_SOURCE_DIR}/build/html/export.html ${CMAKE_SOURCE_DIR}/build/bin/tic80.js ${CMAKE_SOURCE_DIR}/build/bin/tic80.wasm
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Embedding HTML templates for offline export"
        )
        set(EMBEDDED_HTML_TEMPLATES ${CMAKE_CURRENT_BINARY_DIR}/embedded_html_templates.h)
        message(STATUS "htmllocal export enabled - using pre-built web artifacts")
    else()
        message(FATAL_ERROR "BUILD_HTMLLOCAL_EXPORT is ON but web artifacts not found in build/ directory")
    endif()
else()
    # Create placeholder file
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/embedded_html_templates.h "// No embedded HTML templates - BUILD_HTMLLOCAL_EXPORT disabled\n")
    set(EMBEDDED_HTML_TEMPLATES ${CMAKE_CURRENT_BINARY_DIR}/embedded_html_templates.h)
endif()

add_library(tic80studio STATIC
    ${TIC80STUDIO_SRC}
    ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat
    ${EMBEDDED_HTML_TEMPLATES})

target_include_directories(tic80studio
    PRIVATE ${THIRDPARTY_DIR}/jsmn
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(tic80studio PUBLIC tic80core PRIVATE zip wave_writer argparse giflib png)

if(USE_NAETT)
    target_compile_definitions(tic80studio PRIVATE USE_NAETT)
    target_link_libraries(tic80studio PRIVATE naett)
endif()

if(BUILD_PRO)
    target_compile_definitions(tic80studio PRIVATE TIC80_PRO)
endif()

if(BUILD_SDLGPU)
    target_compile_definitions(tic80studio PUBLIC CRT_SHADER_SUPPORT)
endif()

if(BUILD_EDITORS)
    target_compile_definitions(tic80studio PUBLIC BUILD_EDITORS)
endif()
