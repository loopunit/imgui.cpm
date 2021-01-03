file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/SPIRV_Tools_cpm_toolchain_build)

set(SPIRV_Tools_cpm_toolchain_exists false)
if(EXISTS "${CPM_TOOLCHAIN_CACHE}/SPIRV_Tools" AND IS_DIRECTORY "${CPM_TOOLCHAIN_CACHE}/SPIRV_Tools")
    set(SPIRV_Tools_cpm_toolchain_exists true)
endif()

if(NOT SPIRV_Tools_cpm_toolchain_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CMAKE_BINARY_DIR}/.cpm-runtime-cache
		    -S "${CMAKE_CURRENT_LIST_DIR}/spirv_tools" 
		    -B "${CMAKE_BINARY_DIR}/SPIRV_Tools_cpm_toolchain_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/SPIRV_Tools_cpm_toolchain_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target SPIRV_Tools_cpm_toolchain
            --config Release
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/SPIRV_Tools_cpm_toolchain_build")
endif()