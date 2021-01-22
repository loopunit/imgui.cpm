add_cpm_module(imgui_app_fw)

add_library(cpm_runtime::imgui_addons STATIC IMPORTED)
set_target_properties(cpm_runtime::imgui_addons PROPERTIES IMPORTED_LOCATION ${imgui_app_fw_ROOT}/lib/imgui_addons.lib)

add_library(cpm_runtime::imgui_app_fw STATIC IMPORTED)
target_include_directories(cpm_runtime::imgui_app_fw INTERFACE ${imgui_app_fw_ROOT}/include)
target_link_libraries(cpm_runtime::imgui_app_fw INTERFACE cpm_runtime::imgui_addons cpm_runtime::imgui cpm_runtime::glfw cpm_runtime::framegraph cpm_runtime::glslang cpm_runtime::spirv cpm_runtime::basis_universal)
set_target_properties(cpm_runtime::imgui_app_fw PROPERTIES IMPORTED_LOCATION ${imgui_app_fw_ROOT}/lib/imgui_app_fw.lib)
