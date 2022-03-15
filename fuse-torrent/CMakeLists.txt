cmake_minimum_required(VERSION 3.16)
project(FuseTorrent)

set(CMAKE_CXX_STANDARD 17)


macro(_find_files variable_name files_regexp)
    file(GLOB_RECURSE ${variable_name}
         LIST_DIRECTORIES false
         ${files_regexp})
endmacro(_find_files)

macro(_create_source_groups files_list root_path groups_prefix)
    if(WIN32)
        foreach(_file IN ITEMS ${files_list})
            get_filename_component(_file_path "${_file}" PATH)
            file(RELATIVE_PATH _file_path_rel "${root_path}" "${_file_path}")
            string(REPLACE "/" "\\" _group_path "${_file_path_rel}")
            source_group("${groups_prefix}\\${_group_path}" FILES "${_file}")
        endforeach()
    endif()
endmacro(_create_source_groups)


if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
   message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
   file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/release/0.17/conan.cmake"
                  "${CMAKE_BINARY_DIR}/conan.cmake")
endif()
include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES libtorrent/2.0.1
                               cli11/2.1.2
                               indicators/2.2
                      GENERATORS cmake)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if(WIN32)
    # if you get an error here you should install WinFsp with the developer component
    find_package(WinFsp REQUIRED)
    set(FUSE_LIBRARY WinFsp::WinFsp)
else(WIN32)
    # TODO
    message(ERROR "FUSE library required")
endif(WIN32)

configure_file(".clang-format" ".clang-format" COPYONLY)

_find_files(SOURCES "src/*.c*")
_find_files(HEADERS "src/*.h*")

add_executable(FuseTorrent ${SOURCES} ${HEADERS})
target_link_libraries(FuseTorrent ${CONAN_LIBS} ${FUSE_LIBRARY})

_create_source_groups("${SOURCES}" "${CMAKE_CURRENT_LIST_DIR}/src" "sources")
_create_source_groups("${HEADERS}" "${CMAKE_CURRENT_LIST_DIR}/src" "headers")
