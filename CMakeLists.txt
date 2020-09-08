cmake_minimum_required( VERSION 3.12 )
project(imgui
	VERSION 1.7.8
)

option(IMGUI_BUILD_EXAMPLES "Build examples." OFF)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

####

# TODO: add more flexible install configs to PackageProject
#CPMAddPackage(
#  NAME PackageProject.cmake
#  GITHUB_REPOSITORY TheLartians/PackageProject.cmake
#  VERSION 1.3
#)

set(
  PACKAGE_PROJECT_ROOT_PATH
  "${CMAKE_CURRENT_LIST_DIR}"
  CACHE INTERNAL
  "The path to the PackageProject directory"
)

function(packageProject)
  include(CMakePackageConfigHelpers)
  include(GNUInstallDirs)

  cmake_parse_arguments(
    PROJECT
    ""
    "NAME;VERSION;INCLUDE_DIR;INCLUDE_DESTINATION;BINARY_DIR;COMPATIBILITY;VERSION_HEADER"
    "DEPENDENCIES;HEADERS"
    ${ARGN}
  )

  # handle default arguments
  if (NOT DEFINED PROJECT_COMPATIBILITY)
    set(PROJECT_COMPATIBILITY AnyNewerVersion)
  endif()

  if (DEFINED PROJECT_VERSION_HEADER)
    set(PROJECT_VERSION_INCLUDE_DIR ${PROJECT_BINARY_DIR}/PackageProjectInclude)
    string(TOUPPER ${PROJECT_NAME} UPPERCASE_PROJECT_NAME)
    configure_file(
      ${PACKAGE_PROJECT_ROOT_PATH}/version.h.in
      ${PROJECT_VERSION_INCLUDE_DIR}/${PROJECT_VERSION_HEADER}
      @ONLY
    )
    get_target_property(target_type ${PROJECT_NAME} TYPE)
    if (target_type STREQUAL "INTERFACE_LIBRARY")
      set(VISIBILITY INTERFACE)
    else()
      set(VISIBILITY PUBLIC)
    endif()
    target_include_directories(${PROJECT_NAME} ${VISIBILITY} "$<BUILD_INTERFACE:${PROJECT_VERSION_INCLUDE_DIR}>")
    install(
      DIRECTORY ${PROJECT_VERSION_INCLUDE_DIR}/
      DESTINATION ${PROJECT_INCLUDE_DESTINATION}
    )
  endif()

  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${PROJECT_COMPATIBILITY}
  )

  install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}-${PROJECT_VERSION} COMPONENT Runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}-${PROJECT_VERSION} COMPONENT Development
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}-${PROJECT_VERSION} COMPONENT Runtime
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}-${PROJECT_VERSION} COMPONENT Runtime
    PUBLIC_HEADER DESTINATION ${INCLUDE_DESTINATION} COMPONENT Development
  )

  configure_package_config_file(
    ${PACKAGE_PROJECT_ROOT_PATH}/Config.cmake.in
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )

  install(
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION lib/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )

  install(
    FILES 
      "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
      "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    DESTINATION
      lib/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )

  install(
    FILES ${PROJECT_HEADERS}
    DESTINATION ${PROJECT_INCLUDE_DESTINATION}
  )

  set(${PROJECT_NAME}_VERSION ${PROJECT_VERSION} CACHE INTERNAL "")
endfunction()

####

CPMAddPackage(
	NAME imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG 78f753ffff6205040503c6635d35ef0a70805e3c
	DOWNLOAD_ONLY TRUE
    OPTIONS "CPM_SOURCE_CACHE=${CPM_SOURCE_CACHE}"
)

if( NOT ${imgui_ADDED} )
	message( FATAL_ERROR "Could not find imgui" )
endif()

message( STATUS "Building imgui via: ${imgui_SOURCE_DIR}")

set(headers 
	"${imgui_SOURCE_DIR}/imconfig.h"
	"${imgui_SOURCE_DIR}/imgui.h"
	"${imgui_SOURCE_DIR}/imgui_internal.h"
	"${imgui_SOURCE_DIR}/imstb_rectpack.h"
	"${imgui_SOURCE_DIR}/imstb_textedit.h"
	"${imgui_SOURCE_DIR}/imstb_truetype.h"
)

set(sources 
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
)


add_library(${PROJECT_NAME} STATIC ${sources} ${headers})

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
                 $<INSTALL_INTERFACE:${PROJECT_NAME}-${PROJECT_VERSION}>)

packageProject(
  # the name of the target to export
  NAME ${PROJECT_NAME}
  # the version of the target to export
  VERSION ${PROJECT_VERSION}
  # a temporary directory to create the config files
  BINARY_DIR ${PROJECT_BINARY_DIR}
  # location of the target's public headers
  INCLUDE_DIR ${imgui_SOURCE_DIR}
  # should match the target's INSTALL_INTERFACE include directory
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  # semicolon separated list of the project's dependencies
  DEPENDENCIES ""
  # (optional) create a header containing the version info
  # note that the path should be lowercase 
  VERSION_HEADER "${PROJECT_NAME}/version.h"
  HEADERS ${headers}
)

if(IMGUI_BUILD_EXAMPLES)
    add_executable(imgui_example
        ${imgui_SOURCE_DIR}/examples/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/examples/imgui_impl_win32.h
        ${imgui_SOURCE_DIR}/examples/imgui_impl_dx12.cpp
        ${imgui_SOURCE_DIR}/examples/imgui_impl_dx12.h
        ${imgui_SOURCE_DIR}/examples/example_win32_directx12/main.cpp
    )
    
    target_include_directories(imgui_example 
        PRIVATE 
            ${imgui_SOURCE_DIR}/examples
    )
    
    target_link_libraries(imgui_example 
        PRIVATE 
            imgui
            d3d12 d3dcompiler dxgi
    )
endif()