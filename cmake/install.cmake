################################
# Install
################################

set(CPACK_PACKAGE_NAME "TIC-80")
set(CPACK_PACKAGE_VENDOR "Nesbox")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fantasy computer for making, playing and sharing tiny games.")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_REVISION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TIC-80")

if (APPLE)

    set(CPACK_GENERATOR "Bundle")
    set(CPACK_BUNDLE_NAME "tic80")

    configure_file(${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist.in ${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist)
    set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist)

    set(CPACK_BUNDLE_ICON ${CMAKE_SOURCE_DIR}/build/macosx/tic80.icns)
    set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_BINARY_DIR}/bin/tic80")

    install(CODE "set(CMAKE_INSTALL_LOCAL_ONLY true)")
    include(CPack)
elseif (LINUX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "tic80")
    set(CPACK_DEBIAN_FILE_NAME "tic80.deb")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://tic80.com")
    set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nesbox <grigoruk@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "education")

    if(RPI)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    endif()

    install(CODE "set(CMAKE_INSTALL_LOCAL_ONLY true)")
    include(CPack)
endif()
