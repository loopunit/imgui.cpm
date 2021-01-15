cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  glslang_cpm
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
    NAME glslang
    GITHUB_REPOSITORY KhronosGroup/glslang
	GIT_TAG "8.13.3743"
#    GIT_TAG 6d5916d4a91eb9c3b3be377382e2c1c12443fa11
    DOWNLOAD_ONLY true)
	

if (${CPM_FOR_TOOLCHAIN})
    if (${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be false")
    endif()

    ExternalProject_Add(glslang_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${glslang_SOURCE_DIR}"
        INSTALL_DIR ${CPM_TOOLCHAIN_CACHE}/glslang
        CMAKE_CACHE_ARGS 
		    -DSPIRV_CROSS_ENABLE_TESTS:BOOL=Off
            -DSPIRV-Headers_SOURCE_DIR:PATH=${CPM_TOOLCHAIN_CACHE}/SPIRV_Headers
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()
    
    configure_file(patch_CMakeLists.txt ${glslang_SOURCE_DIR}/CMakeLists.txt COPYONLY)

    ExternalProject_Add(glslang_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${glslang_SOURCE_DIR}"
        INSTALL_DIR ${CPM_RUNTIME_CACHE}/glslang
        CMAKE_CACHE_ARGS 
		    -DENABLE_GLSLANG_BINARIES:BOOL=Off
		    -DSPIRV_CROSS_ENABLE_TESTS:BOOL=Off
		    -DSPIRV_SKIP_TESTS:BOOL=On
		    -DBUILD_TESTING:BOOL=Off
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
endif()
