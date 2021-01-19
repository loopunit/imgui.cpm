cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  basis_universal_cpm
  VERSION 1.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
   "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif()

include(ExternalProject)
include(${CPM_SCRIPTS}/CPM.cmake)

CPMAddPackage(
    NAME basis_universal
    GITHUB_REPOSITORY BinomialLLC/basis_universal
	GIT_TAG ef70ddd7c92222a6369f8b296af42b8f9699708b
    DOWNLOAD_ONLY true)

configure_file(patch_CMakeLists.txt                           ${basis_universal_SOURCE_DIR}/CMakeLists.txt COPYONLY)
configure_file(patch_basisu.h                                 ${basis_universal_SOURCE_DIR}/transcoder/basisu.h COPYONLY)

if (${CPM_FOR_TOOLCHAIN})
    if (${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be false")
    endif()

    ExternalProject_Add(basis_universal_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${basis_universal_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/basis_universal
        CMAKE_CACHE_ARGS 
		    -DBASIS_BUILD_TOOL:BOOL=True
		    -DBASIS_BUILD_LIBRARY:BOOL=True
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()

    ExternalProject_Add(basis_universal_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${basis_universal_SOURCE_DIR}"
        INSTALL_DIR ${CPM_RUNTIME_CACHE}/basis_universal
        CMAKE_CACHE_ARGS 
		    -DBASIS_BUILD_TOOL:BOOL=False
		    -DBASIS_BUILD_LIBRARY:BOOL=True
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
endif()