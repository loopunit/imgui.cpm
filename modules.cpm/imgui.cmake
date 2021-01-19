add_cpm_module(imgui)

add_library(cpm_runtime::imgui STATIC IMPORTED)
target_include_directories(cpm_runtime::imgui INTERFACE ${imgui_ROOT}/include)
set_target_properties(cpm_runtime::imgui PROPERTIES IMPORTED_LOCATION ${imgui_ROOT}/lib/imgui.lib)
