cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  mdbx_cpm
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
    NAME mdbx
    GITHUB_REPOSITORY erthink/libmdbx
    GIT_TAG adc720816970335d6b7ef85e8a08ddcb336f0dd0
    DOWNLOAD_ONLY true)

configure_file(patch_CMakeLists.txt ${mdbx_SOURCE_DIR}/CMakeLists.txt COPYONLY)
configure_file(patch_mdbx.h++ ${mdbx_SOURCE_DIR}/mdbx.h++ COPYONLY)

if (${CPM_FOR_TOOLCHAIN})
    if (${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be false")
    endif()

    ExternalProject_Add(mdbx_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${mdbx_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/mdbx
        CMAKE_CACHE_ARGS 
		    -DMDBX_BUILD_TOOLS:BOOL=On
		    -DBUILD_TESTING:BOOL=Off
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()
    

    ExternalProject_Add(mdbx_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${mdbx_SOURCE_DIR}"
        INSTALL_DIR ${CPM_RUNTIME_CACHE}/mdbx
        CMAKE_CACHE_ARGS 
		    -DMDBX_BUILD_TOOLS:BOOL=Off
		    -DBUILD_TESTING:BOOL=Off
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
endif()
