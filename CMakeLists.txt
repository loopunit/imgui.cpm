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
include(cmake/add_cpm_module.cmake)
include(ExternalProject)

####

CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY loopunit/PackageProject.cmake
  GIT_TAG e5ec20069766f4f078f9f01a86e250e20da0817c)

####

include(modules.cpm/imgui.cmake)

if (IMGUI_BUILD_APP_FW)
    include(modules.cpm/spirv_headers.cmake)
    include(modules.cpm/spirv_tools.cmake)
    include(modules.cpm/spirv_cross.cmake)
    include(modules.cpm/glslang.cmake)
    include(modules.cpm/basis_universal.cmake)
    include(modules.cpm/python.cmake)
    include(modules.cpm/framegraph.cmake)
    include(modules.cpm/glfw.cmake)
    include(modules.cpm/libmdbx.cmake)
    
    set(imgui_fw_SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})
    include(modules.cpm/imgui_app_fw.cmake)
endif()

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