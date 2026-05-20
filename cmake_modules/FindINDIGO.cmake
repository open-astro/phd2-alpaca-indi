# FindINDIGO.cmake
#
# Locates the INDIGO client library + headers. INDIGO is the CloudMakers
# property-bus protocol used as an alternative transport to INDI; the two
# coexist in this project (gated by GUIDE_INDI vs GUIDE_INDIGO).
#
# Outputs:
#   INDIGO_FOUND        - whether INDIGO was located
#   INDIGO_INCLUDE_DIR  - directory containing <indigo/indigo_bus.h>
#   INDIGO_LIBRARIES    - libindigo + transitive deps (resolved per-platform)
#
# Lookup order:
#   1. INDIGO_ROOT (CMake/env var) -- typically points at a built source tree:
#        $INDIGO_ROOT/indigo_libs/indigo/*.h
#        $INDIGO_ROOT/build/lib/libindigo.{a,dylib,so}
#      This is the path used during local development against the INDIGO repo.
#   2. ${CMAKE_SOURCE_DIR}/../indigo  -- sibling checkout of indigo-astronomy/indigo
#   3. System paths /usr/local, /opt/homebrew (for `make install` users)
#
# The library is intentionally not REQUIRED by default: callers that need INDIGO
# should `find_package(INDIGO REQUIRED)`; the rest of the tree can compile out
# the INDIGO transport via the GUIDE_INDIGO / INDIGO_CAMERA / ROTATOR_INDIGO
# macro gates in scopes.h / cameras.h / rotators.h.

if(NOT DEFINED INDIGO_ROOT AND DEFINED ENV{INDIGO_ROOT})
    set(INDIGO_ROOT "$ENV{INDIGO_ROOT}")
endif()

set(_INDIGO_search_paths "")
if(INDIGO_ROOT)
    list(APPEND _INDIGO_search_paths "${INDIGO_ROOT}")
endif()
list(APPEND _INDIGO_search_paths
    "${CMAKE_SOURCE_DIR}/../indigo"
    "${CMAKE_SOURCE_DIR}/../../indigo"
)

# Header lookup: indigo_libs/ holds the <indigo/...> tree the project's own
# CFLAGS use. A `make install` layout drops headers into /usr/local/include.
find_path(INDIGO_INCLUDE_DIR
    NAMES indigo/indigo_bus.h
    HINTS
        ${_INDIGO_search_paths}
    PATH_SUFFIXES
        indigo_libs
        include
    PATHS
        /usr/local/include
        /opt/homebrew/include
)

find_library(INDIGO_LIBRARY
    NAMES indigo
    HINTS
        ${_INDIGO_search_paths}
    PATH_SUFFIXES
        build/lib
        lib
    PATHS
        /usr/local/lib
        /opt/homebrew/lib
)

set(INDIGO_LIBRARIES "${INDIGO_LIBRARY}")

# libindigo links against libusb (and on macOS uses CoreFoundation/IOKit etc.).
# When we use the in-tree build, those dependencies live under build/lib and
# are dragged in transitively by the dylib's install_name references; on a
# system install they live in /usr/local/lib already in the linker search path.
# We don't enumerate them here -- callers that hit unresolved symbols at link
# time should add libusb-1.0 explicitly. The frameworks are macOS-only.
if(APPLE AND INDIGO_LIBRARY)
    list(APPEND INDIGO_LIBRARIES
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(INDIGO
    REQUIRED_VARS INDIGO_INCLUDE_DIR INDIGO_LIBRARY
)

mark_as_advanced(INDIGO_INCLUDE_DIR INDIGO_LIBRARY)
