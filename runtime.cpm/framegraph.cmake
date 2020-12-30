include(ExternalProject)
include(cmake/CPM.cmake)

set(FrameGraph_cpm_toolchain_exists false)
#if(EXISTS "${CPM_RUNTIME_CACHE}/FrameGraph" AND IS_DIRECTORY "${CPM_RUNTIME_CACHE}/FrameGraph")
#    set(FrameGraph_cpm_toolchain_exists true)
#endif()

if(NOT FrameGraph_cpm_toolchain_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
		    -S "${CMAKE_CURRENT_LIST_DIR}/FrameGraph" 
		    -B "${CMAKE_BINARY_DIR}/FrameGraph_cpm_runtime_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/FrameGraph_cpm_runtime_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target FrameGraph_cpm_runtime
            --config ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/FrameGraph_cpm_runtime_build")
endif()

add_library(framegraph_framework_cpm STATIC IMPORTED)
set_target_properties(framegraph_framework_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/framegraph/lib/Framework.lib)

add_library(framegraph_vulkanloader_cpm STATIC IMPORTED)
set_target_properties(framegraph_vulkanloader_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/framegraph/lib/VulkanLoader.lib)

add_library(framegraph_stl_cpm STATIC IMPORTED)
set_target_properties(framegraph_stl_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/framegraph/lib/STL.lib)

add_library(framegraph_pipelinecompiler_cpm STATIC IMPORTED)
set_target_properties(framegraph_pipelinecompiler_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/framegraph/lib/PipelineCompiler.lib)

add_library(framegraph_cpm STATIC IMPORTED)
set_target_properties(framegraph_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/FrameGraph/lib/FrameGraph.lib)
target_include_directories(framegraph_cpm INTERFACE ${CPM_RUNTIME_CACHE}/FrameGraph/include)
target_include_directories(framegraph_cpm INTERFACE ${CPM_RUNTIME_CACHE}/FrameGraph/include/vulkan_loader/1.1)
target_compile_definitions(framegraph_cpm INTERFACE FG_ENABLE_VULKAN=1 FG_ENABLE_VULKAN_MEMORY_ALLOCATOR=1 COMPILER_MSVC=1 FG_STD_VARIANT=1 FG_STD_OPTIONAL=1 FG_STD_STRINGVIEW=1 FG_CACHE_LINE=std::hardware_destructive_interference_size FG_HAS_HASHFN_HashArrayRepresentation=1 FG_HAS_HASHFN_HashBytes=1 FG_HAS_HASHFN_Murmur2OrCityhash=1 FG_VULKAN_TARGET_VERSION="110")
target_link_libraries(framegraph_cpm INTERFACE framegraph_framework_cpm framegraph_vulkanloader_cpm framegraph_stl_cpm framegraph_pipelinecompiler_cpm)
