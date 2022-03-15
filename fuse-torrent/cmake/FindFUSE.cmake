include(FindPackageHandleStandardArgs)

# find includes
find_path(
    FUSE_INCLUDE_DIR
    NAMES fuse/fuse.h
    PATHS /usr/local/include/osxfuse
          /usr/local/include
          /usr/include
)

if (APPLE)
    set(_FUSE_NAMES libosxfuse.dylib fuse)
else (APPLE)
    set(_FUSE_NAMES fuse)
endif (APPLE)
find_library(
    FUSE_LIBRARIES
    NAMES ${_FUSE_NAMES}
    PATHS /lib64 /lib /usr/lib64 /usr/lib
          /usr/local/lib64 /usr/local/lib
          /usr/lib/x86_64-linux-gnu
)

find_package_handle_standard_args(
    FUSE
    FOUND_VAR FUSE_FOUND
    REQUIRED_VARS
        FUSE_INCLUDE_DIR
        FUSE_LIBRARIES)

mark_as_advanced (FUSE_INCLUDE_DIR FUSE_LIBRARIES)

if(FUSE_FOUND AND NOT TARGET FUSE::FUSE)
    add_library(FUSE::FUSE SHARED IMPORTED)
    set_target_properties(FUSE::FUSE PROPERTIES
        IMPORTED_LOCATION "${FUSE_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${FUSE_INCLUDE_DIR}"
    )
endif()
