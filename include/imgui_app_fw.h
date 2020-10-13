#pragma once

#include <imgui.h>

namespace imgui_app_fw
{
	enum class platform
	{
		win32_dx11,
		win32_dx12,
		glfw_vulkan,
	};
	
	bool select_platform(platform p);
	void set_window_title(const char* title);

	bool init();
	bool pump();
	void begin_frame();
	void end_frame(ImVec4 clear_color);
	void destroy();
}
