function(add_cpm_module CPM_MODULE_NAME)
    cmake_parse_arguments(add_cpm "FOR_RUNTIME;FOR_TOOLCHAIN" "" "" ${ARGN})
    
    set(MODULE_NAME ${CPM_MODULE_NAME}_ROOT)

    if (${add_cpm_FOR_TOOLCHAIN})
        message(STATUS "Adding ${CPM_MODULE_NAME} to the toolchain stash.")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_toolchain_build)
        set(${MODULE_NAME} ${CPM_TOOLCHAIN_CACHE}/${CPM_MODULE_NAME})
    else()
        message(STATUS "Adding ${CPM_MODULE_NAME} to the runtime stash.")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_runtime_build)
        set(${MODULE_NAME} ${CPM_RUNTIME_CACHE}/${CPM_MODULE_NAME})
    endif()
    
    set(${CPM_MODULE_NAME}_cpm_exists false)
    if(EXISTS "${${MODULE_NAME}}" AND IS_DIRECTORY "${${MODULE_NAME}}")
        set(${CPM_MODULE_NAME}_cpm_exists true)
    endif()

    get_filename_component(CPM_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake" ABSOLUTE)    
    
    if(NOT ${CPM_MODULE_NAME}_cpm_exists)
        if (${add_cpm_FOR_TOOLCHAIN})
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		            -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
                    -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
                    -DCPM_SCRIPTS:PATH=${CPM_SCRIPTS}
                    -DCPM_BUILD_TYPE:STRING=Release
                    -DCPM_FOR_TOOLCHAIN:BOOL=True
                    -DCPM_FOR_RUNTIME:BOOL=False
                    -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
                    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
                    -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
                    -DCPM_RUNTIME_BUILD_CACHE:PATH=${CPM_RUNTIME_BUILD_CACHE}
		            -S "${CMAKE_CURRENT_LIST_DIR}/${CPM_MODULE_NAME}" 
		            -B "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_toolchain_build"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_toolchain_build")

            execute_process(
                COMMAND ${CMAKE_COMMAND} 
		            --build .
	                --target ${CPM_MODULE_NAME}_cpm
                    --config Release
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_toolchain_build")
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
		            -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
                    -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
                    -DCPM_SCRIPTS:PATH=${CPM_SCRIPTS}
                    -DCPM_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
                    -DCPM_FOR_TOOLCHAIN:BOOL=False
                    -DCPM_FOR_RUNTIME:BOOL=True
                    -DCPM_SOURCE_CACHE:PATH=${CPM_SOURCE_CACHE}
                    -DCPM_TOOLCHAIN_CACHE:PATH=${CPM_TOOLCHAIN_CACHE}
                    -DCPM_RUNTIME_CACHE:PATH=${CPM_RUNTIME_CACHE}
                    -DCPM_RUNTIME_BUILD_CACHE:PATH=${CPM_RUNTIME_BUILD_CACHE}
		            -S "${CMAKE_CURRENT_LIST_DIR}/${CPM_MODULE_NAME}" 
		            -B "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_runtime_build"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_runtime_build")

            execute_process(
                COMMAND ${CMAKE_COMMAND} 
		            --build .
	                --target ${CPM_MODULE_NAME}_cpm
                    --config ${CMAKE_BUILD_TYPE}
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CPM_MODULE_NAME}_cpm_runtime_build")
        endif()
    endif()
    
    set(${MODULE_NAME} ${${MODULE_NAME}} PARENT_SCOPE)

endfunction()
