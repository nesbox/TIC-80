# Define our host system
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
set(RPI_HOME $ENV{RPI_HOME})
set(SYSROOT_PATH ${RPI_HOME}/sysroot)

# Define the cross compiler locations
SET(CMAKE_C_COMPILER   ${RPI_HOME}/tools/arm-bcm2708/${CMAKE_LIBRARY_ARCHITECTURE}/bin/${CMAKE_LIBRARY_ARCHITECTURE}-gcc)
SET(CMAKE_CXX_COMPILER ${RPI_HOME}/tools/arm-bcm2708/${CMAKE_LIBRARY_ARCHITECTURE}/bin/${CMAKE_LIBRARY_ARCHITECTURE}-g++)

# Define the sysroot path for the RaspberryPi distribution in our tools folder 
SET(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH})

# Use our definitions for compiler tools
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories only
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )

set(RPI TRUE)
