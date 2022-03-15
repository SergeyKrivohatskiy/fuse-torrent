include(FindPackageHandleStandardArgs)


set(_WIN_FSP_ROOT_DIR "$ENV{ProgramFiles\(x86\)}/WinFsp")

find_path(
    WinFsp_INCLUDE_DIR
    NAMES fuse/fuse.h
    PATHS ${_WIN_FSP_ROOT_DIR}/inc
)
find_file(
    WinFsp_LIBRARY
    NAMES winfsp-x64.dll
    PATHS ${_WIN_FSP_ROOT_DIR}/bin
)
find_library(
    WinFsp_IMPLIB
    NAMES winfsp-x64.lib
    PATHS ${_WIN_FSP_ROOT_DIR}/lib
)


find_package_handle_standard_args(
    WinFsp
    FOUND_VAR WinFsp_FOUND
    REQUIRED_VARS
        WinFsp_LIBRARY
        WinFsp_IMPLIB
        WinFsp_INCLUDE_DIR
)

mark_as_advanced(WinFsp_LIBRARY WinFsp_IMPLIB WinFsp_INCLUDE_DIR)

if(WinFsp_FOUND AND NOT TARGET WinFsp::WinFsp)
    add_library(WinFsp::WinFsp SHARED IMPORTED)
    set_target_properties(WinFsp::WinFsp PROPERTIES
        IMPORTED_LOCATION "${WinFsp_LIBRARY}"
        IMPORTED_IMPLIB "${WinFsp_IMPLIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${WinFsp_INCLUDE_DIR}"
    )
endif()
