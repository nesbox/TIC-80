################################
# bin2txt cart2prj prj2cart xplode wasmp2cart
################################

if(BUILD_TOOLS)

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

    if(LINUX AND NOT SYMBIAN)
        target_link_libraries(xplode m)
    endif()

endif()