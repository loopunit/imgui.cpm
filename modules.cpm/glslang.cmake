add_cpm_module(glslang)

include(${glslang_ROOT}/lib/cmake/HLSLTargets.cmake)
include(${glslang_ROOT}/lib/cmake/OGLCompilerTargets.cmake)
include(${glslang_ROOT}/lib/cmake/OSDependentTargets.cmake)
include(${glslang_ROOT}/lib/cmake/SPVRemapperTargets.cmake)
include(${glslang_ROOT}/lib/cmake/glslangTargets.cmake)
include(${glslang_ROOT}/lib/cmake/SPIRVTargets.cmake)

add_cpm_module(glslang FOR_TOOLCHAIN)