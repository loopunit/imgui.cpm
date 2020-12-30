file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/glslang_cpm_runtime_build)

set(glslang_cpm_runtime_exists false)
if(EXISTS "${CPM_RUNTIME_CACHE}/glslang" AND IS_DIRECTORY "${CPM_RUNTIME_CACHE}/glslang")
    set(glslang_cpm_runtime_exists true)
endif()

if(NOT glslang_cpm_runtime_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
		    -S "${CMAKE_CURRENT_LIST_DIR}/glslang" 
		    -B "${CMAKE_BINARY_DIR}/glslang_cpm_runtime_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/glslang_cpm_runtime_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target glslang_cpm_runtime
            --config ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/glslang_cpm_runtime_build")
endif()

add_library(glslang_hlsl_cpm STATIC IMPORTED)
set_target_properties(glslang_hlsl_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/HLSL.lib)

add_library(glslang_OGLCompiler_cpm STATIC IMPORTED)
set_target_properties(glslang_OGLCompiler_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/OGLCompiler.lib)

add_library(glslang_OSDependent_cpm STATIC IMPORTED)
set_target_properties(glslang_OSDependent_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/OSDependent.lib)

add_library(glslang_SPIRV_cpm STATIC IMPORTED)
set_target_properties(glslang_SPIRV_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/SPIRV.lib)

add_library(glslang_SPVRemapper_cpm STATIC IMPORTED)
set_target_properties(glslang_SPVRemapper_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/SPVRemapper.lib)

add_library(glslang_cpm STATIC IMPORTED)
set_target_properties(glslang_cpm PROPERTIES IMPORTED_LOCATION 
    ${CPM_RUNTIME_CACHE}/glslang/lib/glslang.lib)
    
target_include_directories(glslang_cpm INTERFACE ${CPM_RUNTIME_CACHE}/glslang/include ${CPM_RUNTIME_CACHE}/glslang/include/glslang)
target_compile_definitions(glslang_cpm INTERFACE FG_ENABLE_GLSLANG ENABLE_HLSL)
target_link_libraries(glslang_cpm INTERFACE glslang_hlsl_cpm glslang_OGLCompiler_cpm glslang_OSDependent_cpm glslang_SPIRV_cpm glslang_SPVRemapper_cpm)
