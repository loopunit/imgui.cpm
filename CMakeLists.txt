cmake_minimum_required( VERSION 3.12 )
project(imgui
	VERSION 1.7.8
)

include(CMakeDependentOption)

option(IMGUI_BUILD_EXAMPLES "Build examples." OFF)
cmake_dependent_option(IMGUI_BUILD_APP_FW "Build app framework." OFF "IMGUI_BUILD_EXAMPLES" ON)
option(IMGUI_BUILD_APP_WIN32_DX11 "Win32 DX11" OFF)
option(IMGUI_BUILD_APP_WIN32_DX12 "Win32 DX12" ON)
option(IMGUI_BUILD_APP_GLFW_VULKAN "GLFW Vulkan" OFF)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

####

CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY TheLartians/PackageProject.cmake
  VERSION 1.3
)

####

CPMAddPackage(
	NAME imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG 78f753ffff6205040503c6635d35ef0a70805e3c
	DOWNLOAD_ONLY TRUE
)

if( NOT ${imgui_ADDED} )
	message( FATAL_ERROR "Could not find imgui" )
endif()

message( STATUS "Building imgui via: ${imgui_SOURCE_DIR}")

set(STAGING_ROOT ${CMAKE_BUILD_DIRECTORY}/imgui_staging)

file(GLOB headers 
	"${imgui_SOURCE_DIR}/imconfig.h"
	"${imgui_SOURCE_DIR}/imgui.h"
	"${imgui_SOURCE_DIR}/imgui_internal.h"
	"${imgui_SOURCE_DIR}/imstb_rectpack.h"
	"${imgui_SOURCE_DIR}/imstb_textedit.h"
	"${imgui_SOURCE_DIR}/imstb_truetype.h"
)

file(GLOB sources 
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
)

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
  DEPENDENCIES ""
)

###############################

if(IMGUI_BUILD_APP_FW)
    set(APP_STAGING_ROOT ${CMAKE_BUILD_DIRECTORY}/imgui_app_staging)

    if(IMGUI_BUILD_APP_WIN32_DX12)
        file(GLOB app_headers 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.h"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_dx12.h"
        )
        file(GLOB app_sources 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.cpp"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_dx12.cpp"
        )
    elseif(IMGUI_BUILD_APP_WIN32_DX11)
        file(GLOB app_headers 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.h"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_dx11.h"
        )
        file(GLOB app_sources 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.cpp"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_dx11.cpp"
        )
    elseif(IMGUI_BUILD_APP_GLFW_VULKAN)
        CPMAddPackage(
	        NAME glfw
	        GITHUB_REPOSITORY glfw/glfw
	        GIT_TAG 1353f37a5172d05d89645d8f3e9414fbdd4fc3d6
            OPTIONS
                "BUILD_SHARED_LIBS OFF"
                "GLFW_BUILD_EXAMPLES OFF"
                "GLFW_BUILD_TESTS OFF"
                "GLFW_BUILD_DOCS OFF"
                "GLFW_VULKAN_STATIC ON"
                "USE_MSVC_RUNTIME_LIBRARY_DLL ON"
        )

        find_package(Vulkan REQUIRED)

        file(GLOB app_headers 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.h"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_vulkan.h"
        )
        file(GLOB app_sources 
            "${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.cpp"
            "${imgui_SOURCE_DIR}/examples/imgui_impl_vulkan.cpp"
        )
    endif()

    file(COPY ${app_headers} DESTINATION ${APP_STAGING_ROOT}/include)

    add_library(imgui_app STATIC ${app_sources} ${app_headers})

    target_include_directories(imgui_app PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/examples>
                     $<INSTALL_INTERFACE:imgui_app-${PROJECT_VERSION}>)

    target_link_libraries(imgui_app
        PUBLIC
            imgui
    )

    if(IMGUI_BUILD_APP_WIN32_DX12)
        target_link_libraries(imgui_app
            PUBLIC
                d3d12 d3dcompiler dxgi
        )
    elseif(IMGUI_BUILD_APP_WIN32_DX11)
        target_link_libraries(imgui_app
            PUBLIC
                d3d11 d3dcompiler dxgi
        )
    elseif(IMGUI_BUILD_APP_GLFW_VULKAN)
        target_link_libraries(imgui_app
            PUBLIC
                glfw Vulkan::Vulkan
        )
    endif()

    packageProject(
      # the name of the target to export
      NAME imgui_app
      # the version of the target to export
      VERSION ${PROJECT_VERSION}
      # a temporary directory to create the config files
      BINARY_DIR ${PROJECT_BINARY_DIR}
      # location of the target's public headers
      INCLUDE_DIR ${APP_STAGING_ROOT}/include
      # should match the target's INSTALL_INTERFACE include directory
      INCLUDE_DESTINATION include/imgui_app-${PROJECT_VERSION}
      # semicolon separated list of the project's dependencies
      DEPENDENCIES "imgui"
    )

endif()

################

if(IMGUI_BUILD_EXAMPLES)
    if(IMGUI_BUILD_APP_WIN32_DX12)
        file(GLOB example_sources 
            ${CMAKE_CURRENT_LIST_DIR}/examples/example_win32_directx12/main.cpp
        )
    elseif(IMGUI_BUILD_APP_WIN32_DX11)
        file(GLOB example_sources 
            ${CMAKE_CURRENT_LIST_DIR}/examples/example_win32_directx11/main.cpp
        )
    elseif(IMGUI_BUILD_APP_GLFW_VULKAN)
        file(GLOB example_sources 
            ${CMAKE_CURRENT_LIST_DIR}/examples/example_glfw_vulkan/main.cpp
        )
    endif()

    add_executable(imgui_example
        ${example_sources}
    )
    
    target_include_directories(imgui_example 
        PRIVATE 
            ${imgui_SOURCE_DIR}/examples
    )
    
    target_link_libraries(imgui_example 
        PUBLIC
            imgui_app
    )
endif()