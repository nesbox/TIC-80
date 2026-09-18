# version.cmake — TIC-80 version, derived from git tags (semver).
#
# A build whose HEAD points exactly at a "vMAJOR.MINOR.PATCH" tag is a
# release: it carries that version verbatim (empty VERSION_STATUS). Any
# other build is a development snapshot — it reports the line the repository
# is on, below, with the commit count as the patch, suffixed "-dev". A
# snapshot therefore says which line it belongs to without waiting for the
# next release to be tagged, and stays monotonic while it does.

# The line under development. A snapshot reports it, and a tree with no git
# reports it verbatim — see the no-git note further down. Bump it in the
# commit after a release: v1.2.0 shipped, so main is on the 1.3 line.
set(VERSION_MAJOR 1)
set(VERSION_MINOR 3)
set(VERSION_REVISION 0)
set(VERSION_STATUS "-dev")

string(TIMESTAMP VERSION_YEAR "%Y")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VERSION_BUILD ".dbg")
endif()

# A build with no git at all — a source archive, a distro recipe — cannot be
# a snapshot of anything: it reports the literals above as they stand, as a
# release, so TIC_HOST stays tic80.com instead of sending a shipped build at
# the dev site. Its asset directory follows the same literals, so an archive
# of main asks for /js/v1.3.0/ and /export/v1.3.0/ before that release exists
# — the price of one number carrying both the line under development and this
# fallback. An archive of a release tag carries that tag's literals and asks
# for exactly its own. A git checkout overrides this below.
set(VERSION_IS_RELEASE TRUE)

find_package(Git)
if(Git_FOUND)
    # Everything below asks git about the tree, so the first question is
    # whether git can read it at all: in a source archive, and in a checkout
    # git refuses (the dubious-ownership case), the binary exists but every
    # command fails with an empty answer. Such a build is not a snapshot of
    # anything and keeps the release fallback above — the demotion to a
    # snapshot must not follow from `Git_FOUND` alone, or a shipped build from
    # a tarball would point at the dev site.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_LOG_RESULT
    )

    if(GIT_LOG_RESULT EQUAL 0)
        # Short commit hash for the status line.
        string(SUBSTRING ${GIT_COMMIT_HASH} 0 7 GIT_COMMIT_HASH)
        set(VERSION_HASH ${GIT_COMMIT_HASH})

        # Release: HEAD is exactly a vX.Y.Z tag.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_DESCRIBE
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE GIT_DESCRIBE_RESULT
        )

        if(GIT_DESCRIBE_RESULT EQUAL 0 AND GIT_DESCRIBE MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
            set(VERSION_MAJOR ${CMAKE_MATCH_1})
            set(VERSION_MINOR ${CMAKE_MATCH_2})
            set(VERSION_REVISION ${CMAKE_MATCH_3})
            set(VERSION_STATUS "")
            set(VERSION_IS_RELEASE TRUE)
        else()
            set(VERSION_IS_RELEASE FALSE)

            # Dev snapshots keep a monotonic patch from the commit count.
            # Guarded like the calls above: a repository may still fail these
            # (a shallow or partial clone), in which case we keep the fallback
            # 0 rather than leaving the patch empty.
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
endif()

# The status line and the release flag have to agree: the code branches on
# VERSION_IS_RELEASE to talk to production, and a build that does so must not
# print itself as "-dev". A tree with no git at all kept the default status
# while the flag stayed on, which is exactly the shipped build the fallback
# above exists for.
if(VERSION_IS_RELEASE)
    set(VERSION_STATUS "")
endif()

# The C code branches on this: a dev snapshot talks to the dev site, a release
# to production (see system.h TIC_HOST).
if(VERSION_IS_RELEASE)
    set(VERSION_IS_RELEASE_C 1)
else()
    set(VERSION_IS_RELEASE_C 0)
endif()

# The directory this build's assets live in on the site it talks to —
# /js/<dir>/, /export/<dir>/. Each site lays out the names its own builds ask
# for, and both sides follow from this build alone: a release asks for its tag
# (v1.3.0), which production lays out when that release is deployed, and a
# snapshot asks for the line it is on (1.3), which the dev instance keeps up
# to date from snapshots of main. Nothing here looks at what has been
# released, so a snapshot's export carries the snapshot's engine rather than
# the last release's — which is the whole point of exporting from dev.
if(VERSION_IS_RELEASE)
    set(VERSION_DIR "v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}")
else()
    set(VERSION_DIR "${VERSION_MAJOR}.${VERSION_MINOR}")
endif()
