################################
# bin2txt cart2prj prj2cart xplode wasmp2cart
################################

if(BUILD_DEMO_CARTS)

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

    set(DEMO_CARTS_IN ${CMAKE_SOURCE_DIR}/demos)
    set(DEMO_CARTS_OUT)

    if(BUILD_WITH_LUA)
        list(APPEND DEMO_CARTS
            ${CMAKE_SOURCE_DIR}/config.lua
        )
    endif()

    if(BUILD_WITH_LUA)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/quest.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/car.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/music.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/sfx.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/palette.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bpp.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/tetris.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/font.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/fire.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/benchmark.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/p3d.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/luademo.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/luamark.lua)
    endif()

    if(BUILD_WITH_MOON)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/moondemo.moon)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/moonmark.moon)
    endif()

    if(BUILD_WITH_FENNEL)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/fenneldemo.fnl)
    endif()

    if(BUILD_WITH_JS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/jsdemo.js)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/jsmark.js)
    endif()

    if(BUILD_WITH_SCHEME)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/schemedemo.scm)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/schememark.scm)
    endif()

    if(BUILD_WITH_SQUIRREL)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/squirreldemo.nut)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/squirrelmark.nut)
    endif()

    if(BUILD_WITH_PYTHON)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/pythondemo.py)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/pythonmark.py)
    endif()

    if(BUILD_WITH_WREN)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/wrendemo.wren)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/wrenmark.wren)
    endif()

    if(BUILD_WITH_MRUBY)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/rubydemo.rb)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/rubymark.rb)
    endif()

    if(BUILD_WITH_JANET)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/janetdemo.janet)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/janetmark.janet)
    endif()

    foreach(CART_FILE ${DEMO_CARTS})

        get_filename_component(CART_NAME ${CART_FILE} NAME_WE)

        set(OUTNAME ${CMAKE_SOURCE_DIR}/build/assets/${CART_NAME}.tic.dat)
        set(OUTPRJ ${CMAKE_SOURCE_DIR}/build/${CART_NAME}.tic)

        list(APPEND DEMO_CARTS_OUT ${OUTNAME})

        add_custom_command(OUTPUT ${OUTNAME}
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/prj2cart ${CART_FILE} ${OUTPRJ} && ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${OUTPRJ} ${OUTNAME} -z
            DEPENDS bin2txt prj2cart ${CART_FILE}
    )

    endforeach(CART_FILE)

    if(BUILD_WITH_WASM)

        # we need to build these separately combining both the project
        # and the external WASM binary chunk since projects do not
        # include BINARY chunks

        file(GLOB WASM_DEMOS
            ${DEMO_CARTS_IN}/wasm/*.wasmp
            ${DEMO_CARTS_IN}/bunny/wasmmark/*.wasmp
        )

        foreach(CART_FILE ${WASM_DEMOS})

            get_filename_component(CART_NAME ${CART_FILE} NAME_WE)
            get_filename_component(DIR ${CART_FILE} DIRECTORY)

            set(OUTNAME ${CMAKE_SOURCE_DIR}/build/assets/${CART_NAME}.tic.dat)
            set(WASM_BINARY ${DIR}/${CART_NAME}.wasm)
            set(OUTPRJ ${CMAKE_SOURCE_DIR}/build/${CART_NAME}.tic)
            list(APPEND DEMO_CARTS_OUT ${OUTNAME})
            add_custom_command(OUTPUT ${OUTNAME}
                COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/wasmp2cart ${CART_FILE} ${OUTPRJ} --binary ${WASM_BINARY} && ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${OUTPRJ} ${OUTNAME} -z
                DEPENDS bin2txt wasmp2cart ${CART_FILE} ${WASM_BINARY}
            )

        endforeach(CART_FILE)

    endif()

    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${CMAKE_SOURCE_DIR}/build/cart.png ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat
        DEPENDS bin2txt ${CMAKE_SOURCE_DIR}/build/cart.png)

endif()