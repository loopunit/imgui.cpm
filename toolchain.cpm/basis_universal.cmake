file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/basis_universal_cpm_toolchain_build)

set(basis_universal_cpm_toolchain_exists false)
if(EXISTS "${CPM_TOOLCHAIN_CACHE}/basis_universal" AND IS_DIRECTORY "${CPM_TOOLCHAIN_CACHE}/basis_universal")
    set(basis_universal_cpm_toolchain_exists true)
endif()

if(NOT basis_universal_cpm_toolchain_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CMAKE_BINARY_DIR}/.cpm-runtime-cache
		    -S "${CMAKE_CURRENT_LIST_DIR}/basis_universal" 
		    -B "${CMAKE_BINARY_DIR}/basis_universal_cpm_toolchain_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/basis_universal_cpm_toolchain_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target basis_universal_cpm_toolchain
            --config Release
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/basis_universal_cpm_toolchain_build")
endif()