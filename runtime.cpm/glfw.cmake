file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/glfw_cpm_runtime_build)

set(glfw_cpm_runtime_exists false)
if(EXISTS "${CPM_RUNTIME_CACHE}/glfw" AND IS_DIRECTORY "${CPM_RUNTIME_CACHE}/glfw")
    set(glfw_cpm_runtime_exists true)
endif()

if(NOT glfw_cpm_runtime_exists)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
            -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
		    -S "${CMAKE_CURRENT_LIST_DIR}/glfw" 
		    -B "${CMAKE_BINARY_DIR}/glfw_cpm_runtime_build"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/glfw_cpm_runtime_build")

    execute_process(
        COMMAND ${CMAKE_COMMAND} 
		    --build .
	        --target glfw_cpm_runtime
            --config ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/glfw_cpm_runtime_build")
endif()

add_library(glfw_cpm STATIC IMPORTED)
set_target_properties(glfw_cpm PROPERTIES IMPORTED_LOCATION ${CPM_RUNTIME_CACHE}/glfw/lib/glfw3.lib)
target_include_directories(glfw_cpm INTERFACE ${CPM_RUNTIME_CACHE}/glfw/include)
