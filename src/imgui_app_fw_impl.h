#pragma once

#include "imgui_app_fw.h"

#if IMGUI_APP_WIN32_DX11
bool init_gui_win32_dx11();
bool pump_gui_win32_dx11();
void begin_frame_gui_win32_dx11();
void end_frame_gui_win32_dx11(ImVec4 clear_color);
void destroy_gui_win32_dx11();
#endif

#if IMGUI_APP_WIN32_DX12
bool init_gui_win32_dx12();
bool pump_gui_win32_dx12();
void begin_frame_gui_win32_dx12();
void end_frame_gui_win32_dx12(ImVec4 clear_color);
void destroy_gui_win32_dx12();
#endif

#if IMGUI_APP_GLFW_VULKAN
bool init_gui_glfw_vulkan();
bool pump_gui_glfw_vulkan();
void begin_frame_gui_glfw_vulkan();
void end_frame_gui_glfw_vulkan(ImVec4 clear_color);
void destroy_gui_glfw_vulkan();
#endif

