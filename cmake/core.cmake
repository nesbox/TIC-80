################################
# TIC-80 core
################################

if(WIN32)
    add_library(dlfcn STATIC ${THIRDPARTY_DIR}/dlfcn/src/dlfcn.c)

    target_include_directories(dlfcn
        INTERFACE 
            ${THIRDPARTY_DIR}/dirent/include 
            ${THIRDPARTY_DIR}/dlfcn/src)
endif()

set(BUILD_DEPRECATED TRUE)

set(TIC80CORE_DIR ${CMAKE_SOURCE_DIR}/src)
set(TIC80CORE_SRC
    ${TIC80CORE_DIR}/core/core.c
    ${TIC80CORE_DIR}/core/draw.c
    ${TIC80CORE_DIR}/core/io.c
    ${TIC80CORE_DIR}/core/sound.c
    ${TIC80CORE_DIR}/tic.c
    ${TIC80CORE_DIR}/cart.c
    ${TIC80CORE_DIR}/tools.c
    ${TIC80CORE_DIR}/zip.c
    ${TIC80CORE_DIR}/tilesheet.c
    ${TIC80CORE_DIR}/script.c
)

if(BUILD_DEPRECATED)
    set(TIC80CORE_SRC ${TIC80CORE_SRC} ${TIC80CORE_DIR}/ext/gif.c)
endif()

add_library(tic80core STATIC ${TIC80CORE_SRC})

if (FREEBSD)
    target_include_directories(tic80core PRIVATE ${SYSROOT_PATH}/usr/local/include)
    target_link_directories(tic80core PRIVATE ${SYSROOT_PATH}/usr/local/lib)
endif()

if(WIN32)
    target_link_libraries(tic80core PUBLIC dlfcn)
endif()

target_include_directories(tic80core
    PRIVATE
        ${THIRDPARTY_DIR}/moonscript
        ${THIRDPARTY_DIR}/fennel
        ${POCKETPY_DIR}/src
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(tic80core PRIVATE blipbuf)

if(BUILD_WITH_ZLIB)
    target_link_libraries(tic80core PRIVATE zlib)
endif()

if(BUILD_STATIC)
    if(BUILD_WITH_LUA)
        target_link_libraries(tic80core PRIVATE lua)
    endif()

    if(BUILD_WITH_MOON)
        target_link_libraries(tic80core PRIVATE moon)
    endif()

    if(BUILD_WITH_FENNEL)
        target_link_libraries(tic80core PRIVATE fennel)
    endif()
    
    if(BUILD_WITH_JS)
        target_link_libraries(tic80core PRIVATE js)
    endif()

    if(BUILD_WITH_SCHEME)
        target_link_libraries(tic80core PRIVATE scheme)
    endif()

    if(BUILD_WITH_SQUIRREL)
        target_link_libraries(tic80core PRIVATE squirrel)
    endif()

    if(BUILD_WITH_PYTHON)
        target_link_libraries(tic80core PRIVATE python)
    endif()

    if(BUILD_WITH_WREN)
        target_link_libraries(tic80core PRIVATE wren)
    endif()

    if(BUILD_WITH_RUBY)
        target_link_libraries(tic80core PRIVATE ruby)
    endif()

    if(BUILD_WITH_JANET)
        target_link_libraries(tic80core PRIVATE janet)
    endif()

    if(BUILD_WITH_WASM)
        target_link_libraries(tic80core PRIVATE wasm)
    endif()

    target_link_libraries(tic80core PRIVATE runtime)

endif()

if(BUILD_DEPRECATED)
    target_compile_definitions(tic80core PRIVATE BUILD_DEPRECATED)
    target_link_libraries(tic80core PRIVATE giflib)
endif()

if(LINUX)
    target_link_libraries(tic80core PRIVATE m dl)
endif()
