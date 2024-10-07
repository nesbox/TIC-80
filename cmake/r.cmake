#################################################
# (R) R.h Rinternals.h Rembedded.h Rexts.h etc. #
#################################################
option(BUILD_WITH_R "R Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_R: ${BUILD_WITH_R}")

if(BUILD_WITH_R AND NOT N3DS AND NOT BAREMETALAPI)
  set(TIC_BUILD_WITH_R TRUE)

  if(NOT BUILD_STATIC)
    include("cmake/renv.cmake")

    set(R_LIB_DIR "/usr/include/R")
    include_directories(SYSTEM "/usr/include/R")
    list(
      APPEND R_SRC
      ${CMAKE_SOURCE_DIR}/src/api/r.c
      ${R_LIB_DIR}/R.h
      ${R_LIB_DIR}/Rembedded.h
      ${R_LIB_DIR}/Rinterface.h
      ${R_LIB_DIR}/Rinternals.h
      ${R_LIB_DIR}/Rconfig.h
      ${R_LIB_DIR}/Rdefines.h
    )

    add_library(r ${TIC_RUNTIME} ${R_SRC})
    set_target_properties(r PROPERTIES PREFIX "")
  endif()

  target_link_libraries(
    r
    PRIVATE runtime
    PUBLIC /usr/lib64/R/lib/libR.so
  )

  set_target_properties(r PROPERTIES LINKER_LANGUAGE CXX)
  target_include_directories(r
    PUBLIC ${R_DIR}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
  )

endif()
