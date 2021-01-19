add_cpm_module(glslang FOR_TOOLCHAIN)

#include(${glslang_ROOT}/lib/cmake/HLSLTargets.cmake)
#include(${glslang_ROOT}/lib/cmake/OGLCompilerTargets.cmake)
#include(${glslang_ROOT}/lib/cmake/OSDependentTargets.cmake)
#include(${glslang_ROOT}/lib/cmake/SPVRemapperTargets.cmake)
#include(${glslang_ROOT}/lib/cmake/glslangTargets.cmake)
#include(${glslang_ROOT}/lib/cmake/SPIRVTargets.cmake)
#add_library(cpm_toolchain::glslang ALIAS glslang)
#add_library(cpm_toolchain::spirv ALIAS SPIRV)

add_executable(cpm_toolchain::glslangValidator IMPORTED GLOBAL)
set_target_properties(cpm_toolchain::glslangValidator PROPERTIES IMPORTED_LOCATION ${glslang_ROOT}/bin/glslangValidator.exe)

add_executable(cpm_toolchain::spirv-remap IMPORTED GLOBAL)
set_target_properties(cpm_toolchain::spirv-remap PROPERTIES IMPORTED_LOCATION ${glslang_ROOT}/bin/spirv-remap.exe)

#

add_cpm_module(glslang)

include(${glslang_ROOT}/lib/cmake/HLSLTargets.cmake)
include(${glslang_ROOT}/lib/cmake/OGLCompilerTargets.cmake)
include(${glslang_ROOT}/lib/cmake/OSDependentTargets.cmake)
include(${glslang_ROOT}/lib/cmake/SPVRemapperTargets.cmake)
include(${glslang_ROOT}/lib/cmake/glslangTargets.cmake)
include(${glslang_ROOT}/lib/cmake/SPIRVTargets.cmake)
add_library(cpm_runtime::glslang ALIAS glslang)
add_library(cpm_runtime::spirv ALIAS SPIRV)