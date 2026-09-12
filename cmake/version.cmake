# version.cmake — TIC-80 version, derived from git tags (semver).
#
# A build whose HEAD points exactly at a "vMAJOR.MINOR.PATCH" tag is a
# release: it carries that version verbatim (empty VERSION_STATUS). Any
# other build is a development snapshot — it keeps the last release's
# major/minor and uses the commit count as the patch, suffixed "-dev", so
# the version stays monotonic without needing a tag.

set(VERSION_MAJOR 1)
set(VERSION_MINOR 2)
set(VERSION_REVISION 0)
set(VERSION_STATUS "-dev")

# The release tag, "v<major>.<minor>.<revision>", and the string every path
# is built from — /js/<tag>/, /export/<tag>/. A dev build takes the tag of
# the last release, so a snapshot asks for assets that exist instead of a
# directory named after its own 1.2.<commits>-dev version.
set(VERSION_TAG "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}")

string(TIMESTAMP VERSION_YEAR "%Y")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VERSION_BUILD ".dbg")
endif()

# A build with no git at all — a source tarball, a distro recipe — cannot be
# a snapshot of anything, and the fallback literals above are a release's, so
# it is treated as one: TIC_HOST stays tic80.com instead of sending a shipped
# build at the dev site. A git checkout overrides this below.
set(VERSION_IS_RELEASE TRUE)

find_package(Git)
if(Git_FOUND)
    # Release: HEAD is exactly a vX.Y.Z tag.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_DESCRIBE_RESULT
    )

    set(VERSION_IS_RELEASE FALSE)
    if(GIT_DESCRIBE_RESULT EQUAL 0 AND GIT_DESCRIBE MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_REVISION ${CMAKE_MATCH_3})
        set(VERSION_STATUS "")
        set(VERSION_IS_RELEASE TRUE)
        set(VERSION_TAG "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}")
    endif()

    # Short commit hash for the status line (skip when the tree is not a
    # git checkout, e.g. a release source archive built with git present).
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_LOG_RESULT
    )
    if(GIT_LOG_RESULT EQUAL 0)
        string(SUBSTRING ${GIT_COMMIT_HASH} 0 7 GIT_COMMIT_HASH)
        set(VERSION_HASH ${GIT_COMMIT_HASH})
    endif()

    # Dev snapshots keep a monotonic patch from the commit count. Guarded
    # like the calls above: a container build may see a checked-out tree it
    # cannot read as a repo (dubious ownership), in which case git fails and
    # we keep the fallback 0 rather than leaving the patch empty.
    if(NOT VERSION_IS_RELEASE)
        # Track the last release's major/minor so a post-release commit never
        # reports an *older* version than the tag it follows (e.g. 1.2.<n>-dev
        # right after v1.3.0). Falls back to the defaults when no tag exists.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_LAST_TAG
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_LAST_TAG_RESULT
        )
        if(GIT_LAST_TAG_RESULT EQUAL 0 AND GIT_LAST_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(VERSION_MAJOR ${CMAKE_MATCH_1})
            set(VERSION_MINOR ${CMAKE_MATCH_2})
            # the assets a snapshot talks to are the last release's
            set(VERSION_TAG "${GIT_LAST_TAG}")
        endif()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_REVISION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_REVLIST_RESULT
        )
        if(NOT GIT_REVLIST_RESULT EQUAL 0)
            set(VERSION_REVISION 0)
        endif()
    endif()
endif()

# The C code branches on this: a dev snapshot talks to the dev site and asks
# it for the release-style paths (see system.h TIC_HOST).
if(VERSION_IS_RELEASE)
    set(VERSION_IS_RELEASE_C 1)
else()
    set(VERSION_IS_RELEASE_C 0)
endif()
