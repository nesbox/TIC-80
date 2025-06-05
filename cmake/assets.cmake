macro(BIN2HEX PATH)
    get_filename_component(NAME ${PATH} NAME)

    list(APPEND ASSETS_DEPS ${PROJECT_SOURCE_DIR}/build/${NAME}.dat)

    add_custom_command(OUTPUT ${PROJECT_SOURCE_DIR}/build/${NAME}.dat
        COMMAND ${CMAKE_COMMAND} -DIN=${PATH} -DOUT=${PROJECT_SOURCE_DIR}/build/${NAME}.dat -P ${PROJECT_SOURCE_DIR}/cmake/bin2hex.cmake
        DEPENDS ${PATH})
endmacro()

file(GLOB ASSETS ${PROJECT_SOURCE_DIR}/build/assets/*)

foreach(ASSET ${ASSETS})
    BIN2HEX(${ASSET})
endforeach(ASSET)
