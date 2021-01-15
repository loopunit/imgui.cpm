cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  python_cpm
  VERSION 1.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
   "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif()

include(ExternalProject)
include(${CPM_SCRIPTS}/CPM.cmake)


if (${CPM_FOR_TOOLCHAIN})
    if (${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be false")
    endif()

    CPMAddPackage(
        NAME cpython
        GITHUB_REPOSITORY python/cpython
        GIT_TAG v3.6.7
        DOWNLOAD_ONLY true)

    CPMAddPackage(
        NAME python-cmake-buildsystem
        GITHUB_REPOSITORY loopunit/python-cmake-buildsystem
        VERSION 3.6.7
        GIT_TAG 3eff883aeef96b3af98a9c9632b12ba721b51e8b
        DOWNLOAD_ONLY true)

    ExternalProject_Add(python_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${python-cmake-buildsystem_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/python
        CMAKE_CACHE_ARGS 
            -DDOWNLOADED_SOURCES:PATH=${cpython_SOURCE_DIR}
		    -DDOWNLOAD_SOURCES:BOOL=false
            -DINSTALL_MANUAL:BOOL=false
            -DINSTALL_TEST:BOOL=false
            -DBUILD_WININST:BOOL=false
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE} 
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()

endif()
