cmake_minimum_required( VERSION 3.12 )

project(imgui_app_fw
	VERSION 1.7.8)
	
include(ExternalProject)

CPMAddPackage(
  NAME PackageProject.cmake
  GITHUB_REPOSITORY loopunit/PackageProject.cmake
  GIT_TAG e5ec20069766f4f078f9f01a86e250e20da0817c)

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
		"${imgui_fw_SOURCE_ROOT}/src/win32_dx12/imgui_impl_dx12.h")
	file(GLOB app_fw_impl_sources2
		"${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
		"${imgui_fw_SOURCE_ROOT}/src/win32_dx12/imgui_impl_dx12.cpp"
		"${imgui_fw_SOURCE_ROOT}/src/win32_dx12/main.cpp")
	
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
		"${imgui_fw_SOURCE_ROOT}/src/win32_dx11/main.cpp")

	list(APPEND app_fw_impl_headers ${app_fw_impl_headers2})
	list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
endif()

if(IMGUI_BUILD_APP_GLFW_VULKAN)
	file(GLOB app_fw_impl_sources2 
		"${imgui_fw_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.h"
		"${imgui_fw_SOURCE_ROOT}/src/glfw_vulkan/VulkanDevice2.cpp"
		"${imgui_fw_SOURCE_ROOT}/src/glfw_vulkan/main.cpp")

	list(APPEND app_fw_impl_sources ${app_fw_impl_sources2})
endif()

file(GLOB app_fw_headers
	"${imgui_fw_SOURCE_ROOT}/include/*.h")

file(GLOB app_fw_sources
	"${imgui_fw_SOURCE_ROOT}/src/*.cpp")

add_library(imgui_app_fw STATIC ${app_fw_sources} ${app_fw_impl_sources} ${app_fw_headers} ${app_fw_impl_headers})

target_include_directories(imgui_app_fw 
	PRIVATE 
		${imgui_SOURCE_DIR}/backends)

if(IMGUI_BUILD_APP_WIN32_DX12)
	target_include_directories(imgui_app_fw PRIVATE ${D3D12MemoryAllocator_SOURCE_DIR}/src)
endif()

target_include_directories(imgui_app_fw PUBLIC $<BUILD_INTERFACE:${imgui_fw_SOURCE_ROOT}/include>
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
  INCLUDE_DIR ${imgui_fw_SOURCE_ROOT}/include
  # should match the target's INSTALL_INTERFACE include directory
  INCLUDE_DESTINATION include
  # semicolon separated list of the project's dependencies
  DEPENDENCIES "imgui")
