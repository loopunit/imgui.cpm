add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/imgui_app_fw)

add_library(cpm_runtime::imgui_app_fw ALIAS imgui_app_fw)