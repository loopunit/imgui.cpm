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

CPMAddPackage(
  NAME cpmpckg
  GITHUB_REPOSITORY loopunit/cpmpckg
  GIT_TAG 452b76dd59a587f36c334b275117422c52cdc3cb)

####

set(CPM_USE_glslang true)
set(CPM_USE_basis_universal true)
set(CPM_USE_glfw true)
set(CPM_USE_framegraph true)
set(CPM_USE_imgui true)
include(${cpmpckg_SOURCE_DIR}/CMakeLists.txt)

set(imgui_app_fw_SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})

file(GLOB app_fw_impl_headers)
file(GLOB app_fw_impl_sources)

CPMAddPackage(
	NAME imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG ac08593b9645aee7e086b1e9b98a6a1d79d09210
	DOWNLOAD_ONLY TRUE)

if(NOT ${imgui_ADDED})
	message( FATAL_ERROR "Could not find imgui" )
endif()

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
		"${imgui_app_fw_SOURCE_ROOT}/src/win32_dx12/imgui_impl_dx12.h")
	file(GLOB app_fw_impl_sources2
		"${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
		"${imgui_app_fw_SOURCE_ROOT}/src/win32_dx12/imgui_impl_dx12.cpp"
		"${imgui_app_fw_SOURCE_ROOT}/src/win32_dx12/main.cpp")
	
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
		"${imgui_app_fw_SOURCE_ROOT}/src/win32_dx11/main.cpp")

	list(APPEND app_fw_impl_headers ${app_fw_impl_headers2})
	list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
endif()

if(IMGUI_BUILD_APP_GLFW_VULKAN)
	file(GLOB app_fw_impl_sources2 
		"${imgui_app_fw_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.h"
		"${imgui_app_fw_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.cpp"
		"${imgui_app_fw_SOURCE_ROOT}/src/glfw_vulkan/main.cpp")

	list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
endif()

file(GLOB app_fw_headers
	"${imgui_app_fw_SOURCE_ROOT}/include/*.h")

file(GLOB app_fw_sources
	"${imgui_app_fw_SOURCE_ROOT}/src/*.cpp")

add_library(imgui_app_fw STATIC ${app_fw_sources} ${app_fw_impl_sources} ${app_fw_headers} ${app_fw_impl_headers})

target_include_directories(imgui_app_fw 
	PRIVATE 
		${imgui_SOURCE_DIR}/backends)

if(IMGUI_BUILD_APP_WIN32_DX12)
	target_include_directories(imgui_app_fw PRIVATE ${D3D12MemoryAllocator_SOURCE_DIR}/src)
endif()

target_include_directories(imgui_app_fw PUBLIC $<BUILD_INTERFACE:${imgui_app_fw_SOURCE_ROOT}/include>
				 $<INSTALL_INTERFACE:imgui_app_fw-${PROJECT_VERSION}>)

target_link_libraries(imgui_app_fw
	PUBLIC
		cpm_runtime::imgui)

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
			cpm_runtime::glfw cpm_runtime::framegraph cpm_runtime::glslang cpm_runtime::spirv cpm_runtime::basis_universal)

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
  INCLUDE_DIR ${imgui_app_fw_SOURCE_ROOT}/include
  # should match the target's INSTALL_INTERFACE include directory
  INCLUDE_DESTINATION include
  # semicolon separated list of the project's dependencies
  DEPENDENCIES "imgui")

add_library(cpm_runtime::imgui_app_fw ALIAS imgui_app_fw)

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
            cpm_runtime::imgui_app_fw)
endif()

CPMAddPackage(
	NAME implot
	GITHUB_REPOSITORY epezent/implot
	GIT_TAG 862b8c44ea6d62d736e320b7a11be8438ecbac5d
	DOWNLOAD_ONLY true)

if (NOT implot_ADDED)
	message( FATAL_ERROR "Could not find implot" )
endif()

file(GLOB implot_public_headers 
	"${implot_SOURCE_DIR}/implot.h"
	"${implot_SOURCE_DIR}/implot_internal.h"
)

file(GLOB implot_source
	"${implot_SOURCE_DIR}/*.cpp"
)

#############

CPMAddPackage(
	NAME ImGuiColorTextEdit
	GITHUB_REPOSITORY BalazsJako/ImGuiColorTextEdit
	GIT_TAG 0a88824f7de8d0bd11d8419066caa7d3469395c4
	DOWNLOAD_ONLY true)

if (NOT ImGuiColorTextEdit_ADDED)
	message( FATAL_ERROR "Could not find ImGuiColorTextEdit" )
endif()

file(GLOB ImGuiColorTextEdit_public_headers 
	"${ImGuiColorTextEdit_SOURCE_DIR}/TextEditor.h"
)

file(GLOB ImGuiColorTextEdit_source
	"${ImGuiColorTextEdit_SOURCE_DIR}/*.cpp"
)

#############

CPMAddPackage(
	NAME imgui_markdown
	GITHUB_REPOSITORY juliettef/imgui_markdown
	GIT_TAG 2b33c435b4463999438a5532f1dc201c142a2aab
	DOWNLOAD_ONLY true)

if (NOT imgui_markdown_ADDED)
	message( FATAL_ERROR "Could not find imgui_markdown" )
endif()

file(GLOB imgui_markdown_public_headers 
	"${imgui_markdown_SOURCE_DIR}/imgui_markdown.h"
)

#############

CPMAddPackage(
	NAME imgui_node_editor
	GITHUB_REPOSITORY thedmd/imgui-node-editor
	GIT_TAG 687a72f940c76cf5064e13fe55fa0408c18fcbe4
	DOWNLOAD_ONLY true)

if (NOT imgui_node_editor_ADDED)
	message( FATAL_ERROR "Could not find imgui_node_editor" )
endif()

file(GLOB imgui_node_editor_public_headers 
	"${imgui_node_editor_SOURCE_DIR}/*.h"
	"${imgui_node_editor_SOURCE_DIR}/*.inl"
)

file(GLOB imgui_node_editor_source
	"${imgui_node_editor_SOURCE_DIR}/*.cpp"
)

#############

CPMAddPackage(
	NAME imgui_console
	GITHUB_REPOSITORY rmxbalanque/imgui-console
	GIT_TAG a9a1f83b016c3b455f2b88361aa2b79f25f3f961
	DOWNLOAD_ONLY true)
	

if (NOT imgui_console_ADDED)
	message( FATAL_ERROR "Could not find imgui_console" )
endif()

file(GLOB imgui_console_public_headers 
	"${imgui_console_SOURCE_DIR}/include/imgui_console/imgui_console.h"
)

file(GLOB imgui_console_public_headers_csys
	"${imgui_console_SOURCE_DIR}/include/csys/*.*"
)

file(GLOB imgui_console_source
	"${imgui_console_SOURCE_DIR}/src/imgui_console.cpp"
)

#############

CPMAddPackage(
	NAME ImGuiFileDialog
	GITHUB_REPOSITORY aiekick/ImGuiFileDialog
	GIT_TAG 27cf103c3be5fe11084a07c902c4c180d4e0fff6
	DOWNLOAD_ONLY true)

if (NOT ImGuiFileDialog_ADDED)
	message( FATAL_ERROR "Could not find ImGuiFileDialog" )
endif()

file(GLOB ImGuiFileDialog_public_headers 
	"${ImGuiFileDialog_SOURCE_DIR}/ImGuiFileDialog/ImGuiFileDialog.h"
	"${ImGuiFileDialog_SOURCE_DIR}/ImGuiFileDialog/ImGuiFileDialogConfig.h"
	"${ImGuiFileDialog_SOURCE_DIR}/3rdparty/dirent/include/dirent.h")

file(GLOB ImGuiFileDialog_source
	"${ImGuiFileDialog_SOURCE_DIR}/ImGuiFileDialog/*.cpp")

#############

set(STAGING_ROOT ${CMAKE_CURRENT_BINARY_DIR}/imgui_addons_staging)

file(COPY 
	${implot_public_headers}
	${ImGuiColorTextEdit_public_headers}
	${imgui_markdown_public_headers}
	${imgui_node_editor_public_headers}
	${ImGuiFileDialog_public_headers} 
	${imgui_console_public_headers}
	DESTINATION ${STAGING_ROOT}/include)

file(COPY 
	${ImTerm_public_headers}
	DESTINATION ${STAGING_ROOT}/include/ImTerm)

file(COPY 
	${imgui_console_public_headers_csys}
	DESTINATION ${STAGING_ROOT}/include/csys)


file(COPY 
	${implot_source}
	${ImGuiColorTextEdit_source}
	${imgui_node_editor_source}
	${ImGuiFileDialog_source}
	${imgui_console_source}
	DESTINATION ${STAGING_ROOT}/src)

file(GLOB sources ${STAGING_ROOT}/src/*.*)

add_library(imgui_addons STATIC ${sources})

set_target_properties(imgui_addons PROPERTIES CXX_STANDARD 20)

target_link_libraries(imgui_addons PRIVATE cpm_runtime::imgui)

target_include_directories(imgui_addons PUBLIC $<BUILD_INTERFACE:${STAGING_ROOT}/include>
                 $<INSTALL_INTERFACE:imgui_addons-${PROJECT_VERSION}>)

target_compile_definitions(imgui_addons PUBLIC SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)

packageProject(
  # the name of the target to export
  NAME imgui_addons
  # the version of the target to export
  VERSION ${PROJECT_VERSION}
  # a temporary directory to create the config files
  BINARY_DIR ${PROJECT_BINARY_DIR}
  # location of the target's public headers
  INCLUDE_DIR ${STAGING_ROOT}/include
  # should match the target's INSTALL_INTERFACE include directory
  INCLUDE_DESTINATION include
  # semicolon separated list of the project's dependencies
  DEPENDENCIES ""
)

################

if(IMGUI_BUILD_EXAMPLES)
	file(GLOB addon_example_sources 
		${CMAKE_CURRENT_LIST_DIR}/examples/addon_main.cpp)

    add_executable(imgui_addons_example
        ${addon_example_sources})
    
	set_target_properties(imgui_addons_example PROPERTIES CXX_STANDARD 20)

    target_include_directories(imgui_addons_example
        PRIVATE 
            ${imgui_SOURCE_DIR}/examples)
    
    target_link_libraries(imgui_addons_example
        PUBLIC
            cpm_runtime::imgui_app_fw imgui_addons)
endif()