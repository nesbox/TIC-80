#################################################
# (R) R.h Rinternals.h Rembedded.h Rexts.h etc. #
#################################################
option(BUILD_WITH_R "R Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_R: ${BUILD_WITH_R}")

if(BUILD_WITH_R AND NOT N3DS AND NOT BAREMETALAPI)
  if(NOT BUILD_STATIC)
    if(NOT DEFINED ENV{R_HOME} AND DEFINED PREFIX)
      set(R_HOME "${PREFIX}/lib64/R")
    endif()

    if(NOT DEFINED ENV{LD_LIBRARY_PATH} AND NOT LD_LIBRARY_PATH)
      set(LD_LIBRARY_PATH "/usr/include/R:${R_HOME}/lib:${PREFIX}/lib/jvm/jre/lib/server")
    else()
      # Add these entries to the this *PATH variable
      set(LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${PREFIX}/lib64/R/lib:${PREFIX}/lib/jvm/jre/lib/server")
    endif()

    if(NOT DEFINED ENV{R_SHARE_DIR} AND DEFINED ENV{PREFIX})
      set(R_SHARE_DIR "${PREFIX}/share/R")
    else()
      set(R_SHARE_DIR ENV{R_SHARE_DIR})
    endif()

    if(NOT DEFINED ENV{R_DOC_DIR} AND DEFINED ENV{PREFIX})
      set(R_DOC_DIR "${PREFIX}/share/doc/R")
    else()
      set(R_DOC_DIR ENV{R_DOC_DIR})
    endif()

    if(DEFINED ENV{PREFIX} OR DEFINED PREFIX)
      set(R_SRC "${R_HOME}/lib")
      list(APPEND R_SRC "${PREFIX}/lib/jvm/jre/lib/server" R_SHARE_DIR R_DOC_DIR)
    endif()

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

  target_link_libraries(r PRIVATE runtime)

  set_target_properties(r PROPERTIES LINKER_LANGUAGE CXX)
  target_include_directories(r
    PUBLIC ${R_DIR}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
  )

endif()
