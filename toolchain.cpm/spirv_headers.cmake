file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/SPIRV_Headers_cpm_toolchain_build)

set(SPIRV_Headers_cpm_toolchain_exists false)
if(EXISTS "${CPM_TOOLCHAIN_CACHE}/SPIRV_Headers" AND IS_DIRECTORY "${CPM_TOOLCHAIN_CACHE}/SPIRV_Headers")
    set(SPIRV_Headers_cpm_toolchain_exists true)
endif()

if(NOT SPIRV_Headers_cpm_toolchain_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CMAKE_BINARY_DIR}/.cpm-runtime-cache
		    -S "${CMAKE_CURRENT_LIST_DIR}/spirv_headers" 
		    -B "${CMAKE_BINARY_DIR}/SPIRV_Headers_cpm_toolchain_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/SPIRV_Headers_cpm_toolchain_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target SPIRV_Headers_cpm_toolchain
            --config Release
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/SPIRV_Headers_cpm_toolchain_build")
endif()