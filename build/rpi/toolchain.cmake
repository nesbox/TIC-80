# Define our host system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

if(NOT DEFINED RPI_HOME)
	set(RPI_HOME $ENV{RPI_HOME})
endif()

set(SYSROOT_PATH ${RPI_HOME}/sysroot)

# Define the cross compiler locations
set(CMAKE_C_COMPILER   ${RPI_HOME}/tools/arm-bcm2708/${CMAKE_LIBRARY_ARCHITECTURE}/bin/${CMAKE_LIBRARY_ARCHITECTURE}-gcc)
set(CMAKE_CXX_COMPILER ${RPI_HOME}/tools/arm-bcm2708/${CMAKE_LIBRARY_ARCHITECTURE}/bin/${CMAKE_LIBRARY_ARCHITECTURE}-g++)

# Define the sysroot path for the RaspberryPi distribution in our tools folder 
set(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH})

# Use our definitions for compiler tools
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories only
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )

include_directories(${SYSROOT_PATH}/usr/include)
include_directories(${SYSROOT_PATH}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE})

set(RPI TRUE)
