#include "imgui_app_fw_impl.h"

namespace imgui_app
{
	static bool init_gui_null()
	{
		return false;
	}

	static bool pump_gui_null()
	{
		return false;
	}

	static void begin_frame_gui_null()
	{
	}

	static void end_frame_gui_null(ImVec4 clear_color)
	{
	}

	static void destroy_gui_null()
	{
	}

	static auto init_gui_impl = init_gui_null;
	static auto pump_gui_impl = pump_gui_null;
	static auto begin_frame_gui_impl = begin_frame_gui_null;
	static auto end_frame_gui_impl = end_frame_gui_null;
	static auto destroy_gui_impl = destroy_gui_null;

	bool select_platform(platform p)
	{
#if IMGUI_APP_WIN32_DX11
		if (p == platform::win32_dx11)
		{
			init_gui_impl = init_gui_win32_dx11;
			pump_gui_impl = pump_gui_win32_dx11;
			begin_frame_gui_impl = begin_frame_gui_win32_dx11;
			end_frame_gui_impl = end_frame_gui_win32_dx11;
			destroy_gui_impl = destroy_gui_win32_dx11;
			return true;
		}
#endif
#if IMGUI_APP_WIN32_DX12
		if (p == platform::win32_dx12)
		{
			init_gui_impl = init_gui_win32_dx12;
			pump_gui_impl = pump_gui_win32_dx12;
			begin_frame_gui_impl = begin_frame_gui_win32_dx12;
			end_frame_gui_impl = end_frame_gui_win32_dx12;
			destroy_gui_impl = destroy_gui_win32_dx12;
			return true;
		}
#endif
#if IMGUI_APP_GLFW_VULKAN
		if (p == platform::glfw_vulkan)
		{
			init_gui_impl = init_gui_glfw_vulkan;
			pump_gui_impl = pump_gui_glfw_vulkan;
			begin_frame_gui_impl = begin_frame_gui_glfw_vulkan;
			end_frame_gui_impl = end_frame_gui_glfw_vulkan;
			destroy_gui_impl = destroy_gui_glfw_vulkan;
			return true;
		}
#endif
		return false;
	}

	bool init()
	{
		return init_gui_impl();
	}

	bool pump()
	{
		return pump_gui_impl();
	}

	void begin_frame()
	{
		begin_frame_gui_impl();
	}

	void end_frame(ImVec4 clear_color)
	{
		end_frame_gui_impl(clear_color);
	}

	void destroy()
	{
		return destroy_gui_impl();
	}
}

