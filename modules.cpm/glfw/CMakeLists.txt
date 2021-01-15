cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  glfw_cpm
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
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 0b9e48fa3df9c184ff1abfb2452fd1a4b696ecd8
    DOWNLOAD_ONLY true)
	
configure_file(patch_CMakeLists.txt ${glfw_SOURCE_DIR}/CMakeLists.txt COPYONLY)

if (${CPM_FOR_TOOLCHAIN})
    if (${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be false")
    endif()

else()
    if (NOT ${CPM_FOR_RUNTIME})
        message(FATAL_ERROR "CPM_FOR_RUNTIME should be true")
    endif()

    ExternalProject_Add(glfw_cpm
        PREFIX "${CMAKE_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        SOURCE_DIR "${glfw_SOURCE_DIR}"
        INSTALL_DIR ${CPM_RUNTIME_CACHE}/glfw
        CMAKE_CACHE_ARGS 
            -DGLFW_BUILD_EXAMPLES:BOOL=Off
            -DGLFW_BUILD_TESTS:BOOL=Off
            -DGLFW_BUILD_DOCS:BOOL=Off
            -DGLFW_INSTALL:BOOL=On
            -DGLFW_VULKAN_STATIC:BOOL=Off
            -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
endif()
