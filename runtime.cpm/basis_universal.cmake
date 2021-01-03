file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/basis_universal_cpm_runtime_build)

set(basis_universal_cpm_runtime_exists false)
#if(EXISTS "${CPM_RUNTIME_CACHE}/basis_universal" AND IS_DIRECTORY "${CPM_RUNTIME_CACHE}/basis_universal")
#    set(basis_universal_cpm_runtime_exists true)
#endif()

if(NOT basis_universal_cpm_runtime_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
		    -S "${CMAKE_CURRENT_LIST_DIR}/basis_universal" 
		    -B "${CMAKE_BINARY_DIR}/basis_universal_cpm_runtime_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/basis_universal_cpm_runtime_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target basis_universal_cpm_runtime
            --config ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/basis_universal_cpm_runtime_build")
endif()

add_library(basis_universal_cpm STATIC IMPORTED)
target_include_directories(basis_universal_cpm INTERFACE ${CPM_RUNTIME_CACHE}/basis_universal/include)
set_target_properties(basis_universal_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/basis_universal/lib/basis.lib)
