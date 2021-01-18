add_cpm_module(glfw)

include(${glfw_ROOT}/lib/cmake/glfw3/glfw3Config.cmake)
add_library(cpm_runtime::glfw ALIAS glfw)