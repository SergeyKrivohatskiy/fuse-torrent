cmake_minimum_required(VERSION 3.16)
project(FuseTorrent)

set(CMAKE_CXX_STANDARD 17)


macro(_find_files variable_name files_regexp)
    file(GLOB_RECURSE ${variable_name}
         LIST_DIRECTORIES false
         ${files_regexp})
endmacro(_find_files)

macro(_create_source_groups files_list root_path groups_prefix)
    if (WIN32)
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
   file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
                  "${CMAKE_BINARY_DIR}/conan.cmake")
endif()
include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_run(CONANFILE conanfile.txt
                BASIC_SETUP
                BUILD missing)


_find_files(SOURCES "src/*.c*")
_find_files(HEADERS "include/*.h*")

add_executable(FuseTorrent ${SOURCES} ${HEADERS})
target_include_directories(FuseTorrent PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(FuseTorrent ${CONAN_LIBS})

_create_source_groups("${SOURCES}" "${CMAKE_CURRENT_LIST_DIR}/src" "sources")
_create_source_groups("${HEADERS}" "${CMAKE_CURRENT_LIST_DIR}/include" "headers")
