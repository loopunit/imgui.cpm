cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  SPIRV_Cross_cpm
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
        NAME SPIRV_Cross
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
        GIT_TAG e50f7d1ce8e162d0c826e84168cfa234e4de4ec9
        DOWNLOAD_ONLY true)

    ExternalProject_Add(SPIRV_Cross_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${SPIRV_Cross_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/SPIRV_Cross
        CMAKE_CACHE_ARGS 
		    -DENABLE_CTEST:BOOL=Off
		    -DBUILD_TESTING:BOOL=Off
            -DSPIRV-Headers_SOURCE_DIR:PATH=${CPM_TOOLCHAIN_CACHE}/SPIRV_Headers
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE} 
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()
endif()
