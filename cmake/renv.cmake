if(NOT DEFINED ENV{R_HOME} AND DEFINED PREFIX)
  set(R_HOME "${PREFIX}/lib64/R")
endif()

if(NOT DEFINED ENV{LD_LIBRARY_PATH} AND NOT LD_LIBRARY_PATH)
  set(LD_LIBRARY_PATH "/usr/include/R:${R_HOME}/lib:${PREFIX}/lib/jvm/jre/lib/server")
# else()
#   # Add these entries to the this *PATH variable
#   set(LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${PREFIX}/lib64/R/lib:${PREFIX}/lib/jvm/jre/lib/server")
endif()

if(NOT DEFINED ENV{R_SHARE_DIR} AND DEFINED ENV{PREFIX})
  set(R_SHARE_DIR "${PREFIX}/share/R")
endif()

if(NOT DEFINED ENV{R_DOC_DIR} AND DEFINED ENV{PREFIX})
  set(R_DOC_DIR "${PREFIX}/share/doc/R")
endif()

if(DEFINED ENV{PREFIX} OR DEFINED PREFIX)
  set(R_SRC "${R_HOME}/lib")
  list(APPEND R_SRC "${PREFIX}/lib/jvm/jre/lib/server" R_SHARE_DIR R_DOC_DIR)
endif()

configure_file(cmake/renv.h.in ../src/api/renv.h)
