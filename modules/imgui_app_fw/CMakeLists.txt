cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  imgui_app_fw
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
	NAME imgui_app_fw
	GITHUB_REPOSITORY loopunit/imgui.cpm
	GIT_TAG 65b63954c578aea5dac6e0abf97078e12db13888 
	DOWNLOAD_ONLY TRUE)

if(NOT ${imgui_app_fw_ADDED})
	message( FATAL_ERROR "Could not find imgui_app_fw" )
endif()

ExternalProject_Add(imgui_app_fw_cpm
    PREFIX "${CMAKE_BINARY_DIR}"
    DOWNLOAD_COMMAND ""
    SOURCE_DIR "${imgui_app_fw_SOURCE_DIR}"
    INSTALL_DIR ${CPM_RUNTIME_CACHE}/imgui_app_fw
    CMAKE_CACHE_ARGS 
		-DIMGUI_BUILD_APP_FW:BOOL=True
		-DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		-DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
		-DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
		-DCPM_RUNTIME_BUILD_CACHE:PATH=${CPM_RUNTIME_BUILD_CACHE}
        -DCMAKE_BUILD_TYPE:STRING=${CPM_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
		