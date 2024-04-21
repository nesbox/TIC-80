if(BUILD_DEMO_CARTS)

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

endif()