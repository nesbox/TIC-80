################################
# bin2txt cart2prj prj2cart xplode wasmp2cart
################################

if(BUILD_TOOLS)

    # A conversion tool only knows the languages linked into it. projectComment()
    # (src/studio/project.c) walks the built-in script table to find the comment
    # prefix a project file is written with — "--", "#", "\" — and reads that
    # file's <TILES>/<SAMPLES>/<TRACKS>/… sections by it. The table (Scripts[],
    # src/script.c) is built from TIC_RUNTIME_STATIC and the TIC_BUILD_WITH_*
    # defines: a shared build leaves it empty and hands the engine modules to
    # load instead, which a tool never does.
    #
    # What a missing entry costs is silence rather than an error — every section
    # is missed, the whole project file goes into the code chunk, and the carts
    # come out code-only — so a tool that cannot convert a project is refused
    # here rather than in a rewritten build/assets/.
    if(NOT BUILD_STATIC)
        message(FATAL_ERROR
            "BUILD_TOOLS requires -DBUILD_STATIC=ON: without it the tools register no scripts "
            "and convert every project to a code-only cart.")
    endif()

    set(TIC80_TOOLS_LANGS LUA JS RUBY PYTHON WREN SQUIRREL JANET SCHEME FORTH MINISCRIPT MOON YUE FENNEL WASM)
    set(TIC80_TOOLS_LANGS_OFF "")

    foreach(lang ${TIC80_TOOLS_LANGS})
        if(NOT BUILD_WITH_${lang})
            list(APPEND TIC80_TOOLS_LANGS_OFF ${lang})
        endif()
    endforeach()

    if(TIC80_TOOLS_LANGS_OFF)
        message(FATAL_ERROR
            "BUILD_TOOLS requires every language; ${TIC80_TOOLS_LANGS_OFF} is off, and a project in "
            "one of them converts to a code-only cart without a word. -DBUILD_WITH_ALL=ON covers a "
            "build directory that has none yet: in one configured before, the per-language options "
            "kept that first configure's defaults and have to be named one by one.")
    endif()

    set(TOOLS_DIR ${CMAKE_SOURCE_DIR}/build/tools)

    add_executable(cart2prj ${TOOLS_DIR}/cart2prj.c ${CMAKE_SOURCE_DIR}/src/studio/project.c)
    target_include_directories(cart2prj PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(cart2prj tic80core)

    add_executable(prj2cart ${TOOLS_DIR}/prj2cart.c ${CMAKE_SOURCE_DIR}/src/studio/project.c)
    target_include_directories(prj2cart PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(prj2cart tic80core)

    add_executable(wasmp2cart ${TOOLS_DIR}/wasmp2cart.c ${CMAKE_SOURCE_DIR}/src/studio/project.c)
    target_include_directories(wasmp2cart PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(wasmp2cart tic80core)

    add_executable(bin2txt ${TOOLS_DIR}/bin2txt.c)
    target_link_libraries(bin2txt zlib)

    add_executable(xplode
        ${TOOLS_DIR}/xplode.c
        ${CMAKE_SOURCE_DIR}/src/ext/png.c
        ${CMAKE_SOURCE_DIR}/src/studio/project.c)

    target_include_directories(xplode PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(xplode tic80core png)

    if(LINUX)
        target_link_libraries(xplode m)
    endif()

endif()