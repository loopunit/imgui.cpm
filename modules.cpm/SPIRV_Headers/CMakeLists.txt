cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  SPIRV_Headers_cpm
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
	    NAME SPIRV_Headers
	    GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
	    GIT_TAG f027d53ded7e230e008d37c8b47ede7cd308e19d
	    DOWNLOAD_ONLY true)

    ExternalProject_Add(SPIRV_Headers_cpm_toolchain
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${SPIRV_Headers_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/SPIRV_Headers
        CMAKE_CACHE_ARGS 
		    -DSPIRV_HEADERS_SKIP_EXAMPLES:BOOL=On
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE} 
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()

endif()
