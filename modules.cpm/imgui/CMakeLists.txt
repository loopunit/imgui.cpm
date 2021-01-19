cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  imgui_cpm
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
	NAME imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG ac08593b9645aee7e086b1e9b98a6a1d79d09210
	DOWNLOAD_ONLY TRUE)

if(NOT ${imgui_ADDED})
	message( FATAL_ERROR "Could not find imgui" )
endif()

configure_file(patch_CMakeLists.txt ${imgui_SOURCE_DIR}/CMakeLists.txt COPYONLY)

ExternalProject_Add(imgui_cpm
    PREFIX "${CMAKE_BINARY_DIR}"
    DOWNLOAD_COMMAND ""
    SOURCE_DIR "${imgui_SOURCE_DIR}"
    INSTALL_DIR ${CPM_RUNTIME_CACHE}/imgui
    CMAKE_CACHE_ARGS 
        -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)