cmake_minimum_required( VERSION 3.12 )

project(imgui
	VERSION 1.7.8)

include(CMakeDependentOption)

option(IMGUI_BUILD_EXAMPLES "Build examples." OFF)
cmake_dependent_option(IMGUI_BUILD_APP_FW "Build app framework." OFF "IMGUI_BUILD_EXAMPLES" ON)
option(IMGUI_BUILD_APP_WIN32_DX11 "Win32 DX11" OFF)
option(IMGUI_BUILD_APP_WIN32_DX12 "Win32 DX12" OFF)
option(IMGUI_BUILD_APP_GLFW_VULKAN "GLFW Vulkan" ON)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)
include(ExternalProject)

####

CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY TheLartians/PackageProject.cmake
  VERSION 1.3)

####

if(IMGUI_BUILD_APP_GLFW_VULKAN)
    include(toolchain.cpm/python.cmake)
    include(toolchain.cpm/spirv_headers.cmake)
    include(toolchain.cpm/spirv_tools.cmake)
    include(toolchain.cpm/spirv_cross.cmake)
    include(toolchain.cpm/glslang.cmake)
    include(toolchain.cpm/basis_universal.cmake)

    set(Python3_ROOT_DIR ${CPM_TOOLCHAIN_CACHE}/python)
    find_package(Python3 COMPONENTS Interpreter Development)

    include(runtime.cpm/glslang.cmake)
    include(runtime.cpm/framegraph.cmake)
    include(runtime.cpm/glfw.cmake)
    include(runtime.cpm/basis_universal.cmake)
endif()

####

CPMAddPackage(
	NAME imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG ac08593b9645aee7e086b1e9b98a6a1d79d09210
	DOWNLOAD_ONLY TRUE)

if(NOT ${imgui_ADDED})
	message( FATAL_ERROR "Could not find imgui" )
endif()

message(STATUS "Building imgui via: ${imgui_SOURCE_DIR}")

set(STAGING_ROOT ${CMAKE_CURRENT_BINARY_DIR}/imgui_staging)

file(GLOB headers 
	"${imgui_SOURCE_DIR}/imconfig.h"
	"${imgui_SOURCE_DIR}/imgui.h"
	"${imgui_SOURCE_DIR}/imgui_internal.h"
	"${imgui_SOURCE_DIR}/imstb_rectpack.h"
	"${imgui_SOURCE_DIR}/imstb_textedit.h"
	"${imgui_SOURCE_DIR}/imstb_truetype.h")

file(GLOB sources 
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp")

file(COPY ${headers} DESTINATION ${STAGING_ROOT}/include)

add_library(imgui STATIC ${sources} ${headers})

target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
                 $<INSTALL_INTERFACE:imgui-${PROJECT_VERSION}>)

packageProject(
  # the name of the target to export
  NAME imgui
  # the version of the target to export
  VERSION ${PROJECT_VERSION}
  # a temporary directory to create the config files
  BINARY_DIR ${PROJECT_BINARY_DIR}
  # location of the target's public headers
  INCLUDE_DIR ${STAGING_ROOT}/include
  # should match the target's INSTALL_INTERFACE include directory
  INCLUDE_DESTINATION include/imgui-${PROJECT_VERSION}
  # semicolon separated list of the project's dependencies
  DEPENDENCIES "")

###############################

if(IMGUI_BUILD_APP_FW)
    set(APP_STAGING_ROOT ${CMAKE_CURRENT_BINARY_DIR}/imgui_app_fw_staging)

    file(GLOB app_fw_impl_headers)
    file(GLOB app_fw_impl_sources)
    
    if(IMGUI_BUILD_APP_WIN32_DX12)
        CPMAddPackage(
	        NAME D3D12MemoryAllocator
	        GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator
	        GIT_TAG a322398f6cbc84fbb38bebd473592e6250e5e065
	        DOWNLOAD_ONLY TRUE)

        if(NOT ${D3D12MemoryAllocator_ADDED})
    	    message(FATAL_ERROR "Could not find D3D12MemoryAllocator")
        endif()

        list(APPEND app_fw_impl_headers ${D3D12MemoryAllocator_SOURCE_DIR}/src/D3D12MemAlloc.h)
        list(APPEND app_fw_impl_sources ${D3D12MemoryAllocator_SOURCE_DIR}/src/D3D12MemAlloc.cpp)
    endif()

    if(IMGUI_BUILD_APP_WIN32_DX12)
        file(GLOB app_fw_impl_headers2 
            "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.h"
            "${CMAKE_CURRENT_LIST_DIR}/src/win32_dx12/imgui_impl_dx12.h")
        file(GLOB app_fw_impl_sources2
            "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/win32_dx12/imgui_impl_dx12.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/win32_dx12/main.cpp")
        
        list(APPEND app_fw_impl_headers ${app_fw_impl_headers2})
        list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
    endif()

    if(IMGUI_BUILD_APP_WIN32_DX11)
        file(GLOB app_fw_impl_headers2
            "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.h"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.h")
        file(GLOB app_fw_impl_sources2
            "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/win32_dx11/main.cpp")

        list(APPEND app_fw_impl_headers ${app_fw_impl_headers2})
        list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
    endif()

    if(IMGUI_BUILD_APP_GLFW_VULKAN)
        file(GLOB app_fw_impl_sources2 
            "${CMAKE_CURRENT_LIST_DIR}/src/glfw_vulkan/VulkanDevice2.h"
            "${CMAKE_CURRENT_LIST_DIR}/src/glfw_vulkan/VulkanDevice2.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/glfw_vulkan/main.cpp")

        list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
    endif()

    file(GLOB app_fw_headers
        "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

    file(GLOB app_fw_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

    add_library(imgui_app_fw STATIC ${app_fw_sources} ${app_fw_impl_sources} ${app_fw_headers} ${app_fw_impl_headers})

    target_include_directories(imgui_app_fw 
        PRIVATE 
            ${imgui_SOURCE_DIR}/backends
            ${APP_STAGING_ROOT}/include)

    if(IMGUI_BUILD_APP_WIN32_DX12)
        target_include_directories(imgui_app_fw PRIVATE ${D3D12MemoryAllocator_SOURCE_DIR}/src)
    endif()

    target_include_directories(imgui_app_fw PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
                     $<INSTALL_INTERFACE:imgui_app_fw-${PROJECT_VERSION}>)

    target_link_libraries(imgui_app_fw
        PUBLIC
            imgui)

    if(IMGUI_BUILD_APP_WIN32_DX12)
        target_link_libraries(imgui_app_fw
            PUBLIC
                d3d12 d3dcompiler dxgi)
    endif()

    if(IMGUI_BUILD_APP_WIN32_DX12)
        target_compile_definitions(imgui_app_fw
            PUBLIC
                IMGUI_APP_WIN32_DX12=1)
    endif()
    
    if(IMGUI_BUILD_APP_WIN32_DX11)
        target_link_libraries(imgui_app_fw
            PUBLIC
                d3d11 d3dcompiler dxgi)

        target_compile_definitions(imgui_app_fw
            PUBLIC
                IMGUI_APP_WIN32_DX11=1)
    endif()
    
    if(IMGUI_BUILD_APP_GLFW_VULKAN)
        target_link_libraries(imgui_app_fw
            PUBLIC
                glfw_cpm framegraph_cpm glslang_cpm basis_universal_cpm)

        target_compile_definitions(imgui_app_fw
            PUBLIC
                IMGUI_APP_GLFW_VULKAN=1)
    endif()
    
    set_target_properties(imgui_app_fw PROPERTIES CXX_STANDARD 17)

    packageProject(
      # the name of the target to export
      NAME imgui_app_fw
      # the version of the target to export
      VERSION ${PROJECT_VERSION}
      # a temporary directory to create the config files
      BINARY_DIR ${PROJECT_BINARY_DIR}
      # location of the target's public headers
      INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include
      # should match the target's INSTALL_INTERFACE include directory
      INCLUDE_DESTINATION include/imgui_app_fw-${PROJECT_VERSION}
      # semicolon separated list of the project's dependencies
      DEPENDENCIES "imgui")

endif()

################

if(IMGUI_BUILD_EXAMPLES)

    file(GLOB example_sources 
        ${CMAKE_CURRENT_LIST_DIR}/examples/main.cpp)


    add_executable(imgui_example
        ${example_sources})
    
    set_target_properties(imgui_example PROPERTIES CXX_STANDARD 17)
    
    target_include_directories(imgui_example 
        PRIVATE 
            ${imgui_SOURCE_DIR}/examples)
    
    target_link_libraries(imgui_example 
        PUBLIC
            imgui_app_fw)
endif()