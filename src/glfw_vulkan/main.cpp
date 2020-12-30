#define NOMINMAX

#include "../imgui_app_fw_impl.h"

#include <Framework/Vulkan/VulkanDevice.h>
#include <Framework/Vulkan/VulkanSwapchain.h>
#include <framegraph/FG.h>
#include <pipeline_compiler/VPipelineCompiler.h>

#include "ImguiRenderer.h"

#include <array>
#include <limits>
#include <memory>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

bool ImGui_ImplGlfw_Init(GLFWwindow* window, bool install_callbacks);
void ImGui_ImplGlfw_Shutdown();
void ImGui_ImplGlfw_NewFrame();

void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c);
void ImGui_ImplGlfw_MonitorCallback(GLFWmonitor* monitor, int event);

struct imgui_viewport_data
{
	bool m_is_primary{false};

	struct VulkanSurfaceFactory : public FGC::IVulkanSurface
	{
	private:
		GLFWwindow* _window;

	public:
		VulkanSurfaceFactory(GLFWwindow* wnd) : _window{wnd} {}

		virtual ~VulkanSurfaceFactory() {}

		FGC::ArrayView<const char*> GetRequiredExtensions() const override
		{
			uint32_t	 required_extension_count = 0;
			const char** required_extensions	  = glfwGetRequiredInstanceExtensions(&required_extension_count);

			return FGC::ArrayView<const char*>{required_extensions, required_extension_count};
		}

		SurfaceVk_t Create(InstanceVk_t inst) const override
		{
			VkSurfaceKHR surf = VK_NULL_HANDLE;
			glfwCreateWindowSurface(FGC::BitCast<VkInstance>(inst), _window, nullptr, &surf);
			return FGC::BitCast<SurfaceVk_t>(surf);
		}
	};

	FGC::UniquePtr<FGC::VulkanDeviceInitializer> device;
	FG::FrameGraph								 frame_graphs;
	FG::SwapchainID								 swapchain_id;
	FG::ImguiRenderer							 imgui_renderer;

	void init(ImGuiContext* imgui_context, ImGuiViewport* viewport, bool primary)
	{
		auto window				 = (GLFWwindow*)viewport->PlatformHandle;
		auto new_device			 = std::make_unique<FGC::VulkanDeviceInitializer>();
		auto surface_factory	 = FGC::UniquePtr<FGC::IVulkanSurface>(new VulkanSurfaceFactory(window));
		auto required_extensions = surface_factory->GetRequiredExtensions();

		if (primary)
		{
			new_device->CreateInstance(std::move(surface_factory), "app_name", "engine_name", new_device->GetRecomendedInstanceLayers(), required_extensions);
		}
		else
		{
			// NOTE: not threadsafe
			ImGuiViewport* main_viewport	= ImGui::GetMainViewport();
			auto		   primary_instance = (imgui_viewport_data*)main_viewport->RendererUserData;
			assert(primary_instance);
			new_device->SetInstance(std::move(surface_factory), primary_instance->device->GetVkInstance(), required_extensions);
		}

		new_device->ChooseHighPerformanceDevice();
		new_device->CreateLogicalDevice({{VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_SPARSE_BINDING_BIT}, {VK_QUEUE_COMPUTE_BIT}, {VK_QUEUE_TRANSFER_BIT}}, FGC::Default);

		FG::VulkanDeviceInfo vulkan_info;
		{
			vulkan_info.instance	   = FGC::BitCast<FG::InstanceVk_t>(new_device->GetVkInstance());
			vulkan_info.physicalDevice = FGC::BitCast<FG::PhysicalDeviceVk_t>(new_device->GetVkPhysicalDevice());
			vulkan_info.device		   = FGC::BitCast<FG::DeviceVk_t>(new_device->GetVkDevice());

			vulkan_info.maxStagingBufferMemory = ~FGC::BytesU(0);
			vulkan_info.stagingBufferSize	   = FGC::BytesU{8 * 1024 * 1024};

			for (auto& q : new_device->GetVkQueues())
			{
				FG::VulkanDeviceInfo::QueueInfo qi;
				qi.handle	   = FGC::BitCast<FG::QueueVk_t>(q.handle);
				qi.familyFlags = FGC::BitCast<FG::QueueFlagsVk_t>(q.familyFlags);
				qi.familyIndex = q.familyIndex;
				qi.priority	   = q.priority;
				qi.debugName   = q.debugName;

				vulkan_info.queues.push_back(qi);
			}
		}
		frame_graphs = FG::IFrameGraph::CreateFrameGraph(vulkan_info);

		{
			auto compiler = FG::MakeShared<FG::VPipelineCompiler>(vulkan_info.instance, vulkan_info.physicalDevice, vulkan_info.device);
			compiler->SetCompilationFlags(FG::EShaderCompilationFlags::Quiet);
			frame_graphs->AddPipelineCompiler(compiler);
		}

		FG::VulkanSwapchainCreateInfo swapchain_info;
		{
			swapchain_info.surface		 = FGC::BitCast<FG::SurfaceVk_t>(new_device->GetVkSurface());
			swapchain_info.surfaceSize.x = uint32_t(viewport->Size.x);
			swapchain_info.surfaceSize.y = uint32_t(viewport->Size.y);
		}
		swapchain_id = frame_graphs->CreateSwapchain(swapchain_info);

		imgui_renderer.Initialize(imgui_context, frame_graphs);

		m_is_primary			   = primary;
		device					   = std::move(new_device);
		viewport->RendererUserData = this;
	}

	void handle_resize(ImGuiViewport* viewport)
	{
		auto window = (GLFWwindow*)viewport->PlatformHandle;
		int	 new_width, new_height;
		glfwGetWindowSize(window, &new_width, &new_height);

		if (new_width > 0 && new_height > 0)
		{
			FG::VulkanSwapchainCreateInfo swapchain_info;
			swapchain_info.surface		 = FG::BitCast<FG::SurfaceVk_t>(device->GetVkSurface());
			swapchain_info.surfaceSize.x = new_width;
			swapchain_info.surfaceSize.y = new_height;

			frame_graphs->WaitIdle();

			swapchain_id = frame_graphs->CreateSwapchain(swapchain_info, swapchain_id.Release());
		}
	}

	void destroy(ImGuiViewport* viewport)
	{
		imgui_renderer.Deinitialize(frame_graphs);
		frame_graphs->ReleaseResource(swapchain_id);
		frame_graphs->Deinitialize();
		frame_graphs = nullptr;

		device->DestroyLogicalDevice();
		device->DestroyInstance();
		device.reset();
	}

	void render_frame(ImGuiContext* ctx, ImGuiViewport* viewport, ImDrawData* draw_data)
	{
		if (draw_data->TotalVtxCount > 0)
		{
			FG::CommandBuffer cmdbuf = frame_graphs->Begin(FG::CommandBufferDesc{FG::EQueueType::Graphics});
			CHECK_ERR(cmdbuf);
			{
				FG::RawImageID image = cmdbuf->GetSwapchainImage(swapchain_id);

				FG::RGBA32f		  _clearColor{0.45f, 0.55f, 0.60f, 1.00f};
				FG::LogicalPassID pass_id = cmdbuf->CreateRenderPass(FG::RenderPassDesc{FG::int2{FG::float2{draw_data->DisplaySize.x, draw_data->DisplaySize.y}}}
																		 .AddViewport(FG::float2{draw_data->DisplaySize.x, draw_data->DisplaySize.y})
																		 .AddTarget(FG::RenderTargetID::Color_0, image, _clearColor, FG::EAttachmentStoreOp::Store));

				FG::Task draw_ui = imgui_renderer.Draw(draw_data, ctx, cmdbuf, pass_id);
				FG::Unused(draw_ui);
			}

			CHECK_ERR(frame_graphs->Execute(cmdbuf));
			CHECK_ERR(frame_graphs->Flush());
		}
	}
};

struct gui_primary_context
{
private:
	bool init_shared(ImGuiContext* imgui_context)
	{
		// Setup back-end capabilities flags
		ImGuiIO& io			   = ImGui::GetIO();
		io.BackendRendererName = "imgui_impl_glfw_vulkan";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional) // FIXME-VIEWPORT: Actually unfinished..

		// Create a dummy imgui_viewport_data holder for the main viewport,
		// Since this is created and managed by the application, we will only use the ->Resources[] fields.
		ImGuiViewport* main_viewport = ImGui::GetMainViewport();

		auto primary_viewport_data = IM_NEW(imgui_viewport_data);
		primary_viewport_data->init(imgui_context, main_viewport, true);

		// Setup back-end capabilities flags
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGuiPlatformIO& platform_io	  = ImGui::GetPlatformIO();
			platform_io.Renderer_CreateWindow = [](ImGuiViewport* viewport) -> void {
				instance->create_secondary_window(viewport);
			};
			platform_io.Renderer_DestroyWindow = [](ImGuiViewport* viewport) -> void {
				instance->destroy_secondary_window(viewport);
			};
			platform_io.Renderer_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) -> void {
				instance->set_secondary_window_size(viewport, size);
			};
			platform_io.Renderer_RenderWindow = [](ImGuiViewport* viewport, void*) -> void {
				instance->render_secondary_window(viewport);
			};
			platform_io.Renderer_SwapBuffers = [](ImGuiViewport* viewport, void*) -> void {
				instance->swap_secondary_window_buffers(viewport);
			};
		}

		return true;
	}

	void shutdown_shared()
	{
		// Manually delete main viewport render resources in-case we haven't initialized for viewports
		ImGuiViewport*		 main_viewport		= ImGui::GetMainViewport();
		imgui_viewport_data* main_viewport_data = (imgui_viewport_data*)main_viewport->RendererUserData;
		main_viewport->RendererUserData			= nullptr;

		// Clean up windows and device objects
		ImGui::DestroyPlatformWindows();

		// destroy_device_objects();

		main_viewport_data->destroy(main_viewport);
		IM_DELETE(main_viewport_data);

#if 0
		m_device.reset();
		m_allocator.reset();
#endif
	}

	void new_frame()
	{
		ImGuiViewport*		 main_viewport		= ImGui::GetMainViewport();
		imgui_viewport_data* main_viewport_data = (imgui_viewport_data*)main_viewport->RendererUserData;

		if (main_viewport->PlatformRequestResize)
		{
			main_viewport_data->handle_resize(main_viewport);
			main_viewport->PlatformRequestResize = false;
		}
	}

	void create_secondary_window(ImGuiViewport* viewport)
	{
		imgui_viewport_data* data  = IM_NEW(imgui_viewport_data)();
		viewport->RendererUserData = data;
		data->init(ImGui::GetCurrentContext(), viewport, false);
	}

	void destroy_secondary_window(ImGuiViewport* viewport)
	{
		// The main viewport (owned by the application) will always have RendererUserData == nullptr since we didn't create the data for it.
		if (imgui_viewport_data* data = (imgui_viewport_data*)viewport->RendererUserData)
		{
			data->destroy(viewport);
			IM_DELETE(data);
		}
		viewport->RendererUserData = nullptr;
	}

	void set_secondary_window_size(ImGuiViewport* viewport, ImVec2 size)
	{
		imgui_viewport_data* data = (imgui_viewport_data*)viewport->RendererUserData;
		data->handle_resize(viewport);
	}

	void render_secondary_window(ImGuiViewport* viewport)
	{
		const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

		imgui_viewport_data* data = (imgui_viewport_data*)viewport->RendererUserData;
		data->render_frame(ImGui::GetCurrentContext(), viewport, viewport->DrawData);
	}

	void swap_secondary_window_buffers(ImGuiViewport* viewport)
	{
		imgui_viewport_data* data = (imgui_viewport_data*)viewport->RendererUserData;
	}

private:
	ImGuiContext* m_context = nullptr;
	GLFWwindow*	  m_window	= nullptr;
	bool		  m_ready	= false;

	ImVec2 m_position;
	ImVec2 m_size;

public:
	void set_window_title(const char* title)
	{
		glfwSetWindowTitle(m_window, title);
	}

	void destroy_window()
	{
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}

	void show_window()
	{
		glfwShowWindow(m_window);
	}

	void begin_frame()
	{
		new_frame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void end_frame(ImVec4 clear_color)
	{
		ImGui::Render();

		ImGuiViewport*		 main_viewport		= ImGui::GetMainViewport();
		imgui_viewport_data* main_viewport_data = (imgui_viewport_data*)main_viewport->RendererUserData;

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
		}

		main_viewport_data->render_frame(m_context, main_viewport, ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
		}
	}

	gui_primary_context(ImVec2 p, ImVec2 s)
	{
		if (!glfwInit())
			return; // TODO: throw

		{
			m_position = p;
			m_size	   = s;

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			m_window = glfwCreateWindow(int(s.x), int(s.y), "", NULL, NULL);
		}

		if (!create_device(m_window))
		{
			cleanup_device();
			destroy_window();
			throw std::exception("Device creation failed");
		}
	}

	bool init()
	{
		show_window();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();

		m_context	= ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
		// io.ConfigViewportsNoAutoMerge = true;
		// io.ConfigViewportsNoTaskBarIcon = true;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		// ImGui::StyleColorsClassic();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding			  = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_Init(m_window, true);

		init_shared(m_context);

		// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
		// - If the file cannot be loaded, the function will return nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
		// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
		// ImGui_ImplXXXX_NewFrame below will call.
		// - Read 'docs/FONTS.md' for more instructions and details.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
		// io.Fonts->AddFontDefault();
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
		// io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
		// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
		// IM_ASSERT(font != nullptr);

		return true;
	}

	~gui_primary_context()
	{
		wait_for_last_submitted_frame();
		shutdown_shared();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		m_context = nullptr;

		cleanup_device();
		destroy_window();
	}

private:
	bool on_resize(ImVec2 s)
	{
		if (m_ready)
		{
			wait_for_last_submitted_frame();
			// Don't need to do this (unless device changes or resets?) ImGui_ImplDX12_InvalidateDeviceObjects();
			cleanup_render_target();
			resize_swap_chain(m_window, (uint32_t)s.x, (uint32_t)s.y);
			create_render_target();
			// Don't need to do this (unless device changes or resets?) ImGui_ImplDX12_CreateDeviceObjects();
			return true;
		}
		return false;
	}

	bool create_device(GLFWwindow* window)
	{
		return true;
	}

	void cleanup_device() {}

	void create_render_target() {}

	void cleanup_render_target()
	{
		wait_for_last_submitted_frame();
	}

	void wait_for_last_submitted_frame() {}

	void resize_swap_chain(GLFWwindow* window, int width, int height) {}

public:
	bool pump_events()
	{
		glfwPollEvents();
		return !glfwWindowShouldClose(m_window);
	}

	static inline std::unique_ptr<gui_primary_context> instance;
};

void set_window_title_glfw_vulkan(const char* title)
{
	gui_primary_context::instance->set_window_title(title);
}

bool init_gui_glfw_vulkan()
{
	gui_primary_context::instance = std::unique_ptr<gui_primary_context>(new gui_primary_context({100.0f, 100.0f}, {1280.0f, 800.0f}));
	return gui_primary_context::instance->init();
}

void destroy_gui_glfw_vulkan()
{
	gui_primary_context::instance.reset();
}

bool pump_gui_glfw_vulkan()
{
	return gui_primary_context::instance->pump_events();
}

void begin_frame_gui_glfw_vulkan()
{
	gui_primary_context::instance->begin_frame();
}

void end_frame_gui_glfw_vulkan(ImVec4 clear_color)
{
	gui_primary_context::instance->end_frame(clear_color);
}

//////////////////////////////

// Data
static GLFWwindow* g_Window									  = NULL; // Main window
static double	   g_Time									  = 0.0;
static bool		   g_MouseJustPressed[ImGuiMouseButton_COUNT] = {};
static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT]	  = {};
static bool		   g_InstalledCallbacks						  = false;
static bool		   g_WantUpdateMonitors						  = true;

// Chain GLFW callbacks for main viewport: our callbacks will call the user's previously installed callbacks, if any.
static GLFWmousebuttonfun g_PrevUserCallbackMousebutton = NULL;
static GLFWscrollfun	  g_PrevUserCallbackScroll		= NULL;
static GLFWkeyfun		  g_PrevUserCallbackKey			= NULL;
static GLFWcharfun		  g_PrevUserCallbackChar		= NULL;
static GLFWmonitorfun	  g_PrevUserCallbackMonitor		= NULL;

// Forward Declarations
static void ImGui_ImplGlfw_UpdateMonitors();
static void ImGui_ImplGlfw_InitPlatformInterface();
static void ImGui_ImplGlfw_ShutdownPlatformInterface();

static void ImGui_ImplGlfw_WindowSizeCallback(GLFWwindow* window, int, int);

static const char* ImGui_ImplGlfw_GetClipboardText(void* user_data)
{
	return glfwGetClipboardString((GLFWwindow*)user_data);
}

static void ImGui_ImplGlfw_SetClipboardText(void* user_data, const char* text)
{
	glfwSetClipboardString((GLFWwindow*)user_data, text);
}

void ImGui_ImplGlfw_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (g_PrevUserCallbackMousebutton != NULL && window == g_Window)
		g_PrevUserCallbackMousebutton(window, button, action, mods);

	if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(g_MouseJustPressed))
		g_MouseJustPressed[button] = true;
}

void ImGui_ImplGlfw_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (g_PrevUserCallbackScroll != NULL && window == g_Window)
		g_PrevUserCallbackScroll(window, xoffset, yoffset);

	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH += (float)xoffset;
	io.MouseWheel += (float)yoffset;
}

void ImGui_ImplGlfw_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (g_PrevUserCallbackKey != NULL && window == g_Window)
		g_PrevUserCallbackKey(window, key, scancode, action, mods);

	ImGuiIO& io = ImGui::GetIO();
	if (action == GLFW_PRESS)
		io.KeysDown[key] = true;
	if (action == GLFW_RELEASE)
		io.KeysDown[key] = false;

	// Modifiers are not reliable across systems
	io.KeyCtrl	= io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt	= io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
#ifdef _WIN32
	io.KeySuper = false;
#else
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
#endif
}

void ImGui_ImplGlfw_CharCallback(GLFWwindow* window, unsigned int c)
{
	if (g_PrevUserCallbackChar != NULL && window == g_Window)
		g_PrevUserCallbackChar(window, c);

	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(c);
}

void ImGui_ImplGlfw_MonitorCallback(GLFWmonitor*, int)
{
	g_WantUpdateMonitors = true;
}

bool ImGui_ImplGlfw_Init(GLFWwindow* window, bool install_callbacks)
{
	g_Window = window;
	g_Time	 = 0.0;

	// Setup backend capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;		  // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;		  // We can honor io.WantSetMousePos requests (optional, rarely used)
	io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;	  // We can create multi-viewports on the Platform side (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
	io.BackendPlatformName = "imgui_impl_glfw";

	// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
	io.KeyMap[ImGuiKey_Tab]			= GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow]	= GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow]	= GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow]		= GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow]	= GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp]		= GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown]	= GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home]		= GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End]			= GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Insert]		= GLFW_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete]		= GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace]	= GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space]		= GLFW_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter]		= GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape]		= GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
	io.KeyMap[ImGuiKey_A]			= GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C]			= GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V]			= GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X]			= GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y]			= GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z]			= GLFW_KEY_Z;

	io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
	io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
	io.ClipboardUserData  = g_Window;

	// Create mouse cursors
	// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
	// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
	// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
	GLFWerrorfun prev_error_callback			= glfwSetErrorCallback(NULL);
	g_MouseCursors[ImGuiMouseCursor_Arrow]		= glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_TextInput]	= glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeNS]	= glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeEW]	= glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_Hand]		= glfwCreateStandardCursor(GLFW_HAND_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeAll]	= glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
	g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
	glfwSetErrorCallback(prev_error_callback);

	// Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
	g_PrevUserCallbackMousebutton = NULL;
	g_PrevUserCallbackScroll	  = NULL;
	g_PrevUserCallbackKey		  = NULL;
	g_PrevUserCallbackChar		  = NULL;
	g_PrevUserCallbackMonitor	  = NULL;
	if (install_callbacks)
	{
		g_InstalledCallbacks		  = true;
		g_PrevUserCallbackMousebutton = glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
		g_PrevUserCallbackScroll	  = glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
		g_PrevUserCallbackKey		  = glfwSetKeyCallback(window, ImGui_ImplGlfw_KeyCallback);
		g_PrevUserCallbackChar		  = glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
		g_PrevUserCallbackMonitor	  = glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);
	}

	// Update monitors the first time (note: monitor callback are broken in GLFW 3.2 and earlier, see github.com/glfw/glfw/issues/784)
	ImGui_ImplGlfw_UpdateMonitors();
	glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);

	// Our mouse update function expect PlatformHandle to be filled for the main viewport
	ImGuiViewport* main_viewport  = ImGui::GetMainViewport();
	main_viewport->PlatformHandle = (void*)g_Window;
#ifdef _WIN32
	main_viewport->PlatformHandleRaw = glfwGetWin32Window(g_Window);
#endif
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		ImGui_ImplGlfw_InitPlatformInterface();

	glfwSetWindowSizeCallback(g_Window, ImGui_ImplGlfw_WindowSizeCallback);

	return true;
}

void ImGui_ImplGlfw_Shutdown()
{
	ImGui_ImplGlfw_ShutdownPlatformInterface();

	if (g_InstalledCallbacks)
	{
		glfwSetMouseButtonCallback(g_Window, g_PrevUserCallbackMousebutton);
		glfwSetScrollCallback(g_Window, g_PrevUserCallbackScroll);
		glfwSetKeyCallback(g_Window, g_PrevUserCallbackKey);
		glfwSetCharCallback(g_Window, g_PrevUserCallbackChar);
		g_InstalledCallbacks = false;
	}

	for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
	{
		glfwDestroyCursor(g_MouseCursors[cursor_n]);
		g_MouseCursors[cursor_n] = NULL;
	}
}

static void ImGui_ImplGlfw_UpdateMousePosAndButtons()
{
	// Update buttons
	ImGuiIO& io = ImGui::GetIO();
	for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
	{
		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
		io.MouseDown[i]		  = g_MouseJustPressed[i] || glfwGetMouseButton(g_Window, i) != 0;
		g_MouseJustPressed[i] = false;
	}

	// Update mouse position
	const ImVec2 mouse_pos_backup = io.MousePos;
	io.MousePos					  = ImVec2(-FLT_MAX, -FLT_MAX);
	io.MouseHoveredViewport		  = 0;
	ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
	for (int n = 0; n < platform_io.Viewports.Size; n++)
	{
		ImGuiViewport* viewport = platform_io.Viewports[n];
		GLFWwindow*	   window	= (GLFWwindow*)viewport->PlatformHandle;
		IM_ASSERT(window != NULL);
#ifdef __EMSCRIPTEN__
		const bool focused = true;
		IM_ASSERT(platform_io.Viewports.Size == 1);
#else
		const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
#endif
		if (focused)
		{
			if (io.WantSetMousePos)
			{
				glfwSetCursorPos(window, (double)(mouse_pos_backup.x - viewport->Pos.x), (double)(mouse_pos_backup.y - viewport->Pos.y));
			}
			else
			{
				double mouse_x, mouse_y;
				glfwGetCursorPos(window, &mouse_x, &mouse_y);
				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					// Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
					int window_x, window_y;
					glfwGetWindowPos(window, &window_x, &window_y);
					io.MousePos = ImVec2((float)mouse_x + window_x, (float)mouse_y + window_y);
				}
				else
				{
					// Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
					io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
				}
			}
			for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
				io.MouseDown[i] |= glfwGetMouseButton(window, i) != 0;
		}

		// (Optional) When using multiple viewports: set io.MouseHoveredViewport to the viewport the OS mouse cursor is hovering.
		// Important: this information is not easy to provide and many high-level windowing library won't be able to provide it correctly, because
		// - This is _ignoring_ viewports with the ImGuiViewportFlags_NoInputs flag (pass-through windows).
		// - This is _regardless_ of whether another viewport is focused or being dragged from.
		// If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, imgui will ignore this field and infer the information by relying on the
		// rectangles and last focused time of every viewports it knows about. It will be unaware of other windows that may be sitting between or over your windows.
		// [GLFW] FIXME: This is currently only correct on Win32. See what we do below with the WM_NCHITTEST, missing an equivalent for other systems.
		// See https://github.com/glfw/glfw/issues/1236 if you want to help in making this a GLFW feature.
		const bool window_no_input = (viewport->Flags & ImGuiViewportFlags_NoInputs) != 0;
		glfwSetWindowAttrib(window, GLFW_MOUSE_PASSTHROUGH, window_no_input);
		if (glfwGetWindowAttrib(window, GLFW_HOVERED) && !window_no_input)
			io.MouseHoveredViewport = viewport->ID;
	}
}

static void ImGui_ImplGlfw_UpdateMouseCursor()
{
	ImGuiIO& io = ImGui::GetIO();
	if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(g_Window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		return;

	ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
	ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
	for (int n = 0; n < platform_io.Viewports.Size; n++)
	{
		GLFWwindow* window = (GLFWwindow*)platform_io.Viewports[n]->PlatformHandle;
		if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
		{
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		}
		else
		{
			// Show OS mouse cursor
			// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
			glfwSetCursor(window, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}

static void ImGui_ImplGlfw_UpdateGamepads()
{
	ImGuiIO& io = ImGui::GetIO();
	memset(io.NavInputs, 0, sizeof(io.NavInputs));
	if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
		return;

// Update gamepad inputs
#define MAP_BUTTON(NAV_NO, BUTTON_NO)                                                                                                                                              \
	{                                                                                                                                                                              \
		if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS)                                                                                                         \
			io.NavInputs[NAV_NO] = 1.0f;                                                                                                                                           \
	}
#define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1)                                                                                                                                        \
	{                                                                                                                                                                              \
		float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0;                                                                                                                     \
		v		= (v - V0) / (V1 - V0);                                                                                                                                            \
		if (v > 1.0f)                                                                                                                                                              \
			v = 1.0f;                                                                                                                                                              \
		if (io.NavInputs[NAV_NO] < v)                                                                                                                                              \
			io.NavInputs[NAV_NO] = v;                                                                                                                                              \
	}
	int					 axes_count = 0, buttons_count = 0;
	const float*		 axes	 = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
	const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
	MAP_BUTTON(ImGuiNavInput_Activate, 0);	 // Cross / A
	MAP_BUTTON(ImGuiNavInput_Cancel, 1);	 // Circle / B
	MAP_BUTTON(ImGuiNavInput_Menu, 2);		 // Square / X
	MAP_BUTTON(ImGuiNavInput_Input, 3);		 // Triangle / Y
	MAP_BUTTON(ImGuiNavInput_DpadLeft, 13);	 // D-Pad Left
	MAP_BUTTON(ImGuiNavInput_DpadRight, 11); // D-Pad Right
	MAP_BUTTON(ImGuiNavInput_DpadUp, 10);	 // D-Pad Up
	MAP_BUTTON(ImGuiNavInput_DpadDown, 12);	 // D-Pad Down
	MAP_BUTTON(ImGuiNavInput_FocusPrev, 4);	 // L1 / LB
	MAP_BUTTON(ImGuiNavInput_FocusNext, 5);	 // R1 / RB
	MAP_BUTTON(ImGuiNavInput_TweakSlow, 4);	 // L1 / LB
	MAP_BUTTON(ImGuiNavInput_TweakFast, 5);	 // R1 / RB
	MAP_ANALOG(ImGuiNavInput_LStickLeft, 0, -0.3f, -0.9f);
	MAP_ANALOG(ImGuiNavInput_LStickRight, 0, +0.3f, +0.9f);
	MAP_ANALOG(ImGuiNavInput_LStickUp, 1, +0.3f, +0.9f);
	MAP_ANALOG(ImGuiNavInput_LStickDown, 1, -0.3f, -0.9f);
#undef MAP_BUTTON
#undef MAP_ANALOG
	if (axes_count > 0 && buttons_count > 0)
		io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
	else
		io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
}

static void ImGui_ImplGlfw_UpdateMonitors()
{
	ImGuiPlatformIO& platform_io	= ImGui::GetPlatformIO();
	int				 monitors_count = 0;
	GLFWmonitor**	 glfw_monitors	= glfwGetMonitors(&monitors_count);
	platform_io.Monitors.resize(0);
	for (int n = 0; n < monitors_count; n++)
	{
		ImGuiPlatformMonitor monitor;
		int					 x, y;
		glfwGetMonitorPos(glfw_monitors[n], &x, &y);
		const GLFWvidmode* vid_mode = glfwGetVideoMode(glfw_monitors[n]);
		monitor.MainPos = monitor.WorkPos = ImVec2((float)x, (float)y);
		monitor.MainSize = monitor.WorkSize = ImVec2((float)vid_mode->width, (float)vid_mode->height);

		int w, h;
		glfwGetMonitorWorkarea(glfw_monitors[n], &x, &y, &w, &h);
		if (w > 0 && h > 0) // Workaround a small GLFW issue reporting zero on monitor changes: https://github.com/glfw/glfw/pull/1761
		{
			monitor.WorkPos	 = ImVec2((float)x, (float)y);
			monitor.WorkSize = ImVec2((float)w, (float)h);
		}

		// Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at
		// runtime.
		float x_scale, y_scale;
		glfwGetMonitorContentScale(glfw_monitors[n], &x_scale, &y_scale);
		monitor.DpiScale = x_scale;
		platform_io.Monitors.push_back(monitor);
	}
	g_WantUpdateMonitors = false;
}

void ImGui_ImplGlfw_NewFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(
		io.Fonts->IsBuilt() &&
		"Font atlas not built! It is generally built by the renderer backend. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

	// Setup display size (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(g_Window, &w, &h);
	glfwGetFramebufferSize(g_Window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)w, (float)h);
	if (w > 0 && h > 0)
		io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

	if (g_WantUpdateMonitors)
		ImGui_ImplGlfw_UpdateMonitors();

	// Setup time step
	double current_time = glfwGetTime();
	io.DeltaTime		= g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f / 60.0f);
	g_Time				= current_time;

	ImGui_ImplGlfw_UpdateMousePosAndButtons();
	ImGui_ImplGlfw_UpdateMouseCursor();

	// Update game controllers (if enabled and available)
	ImGui_ImplGlfw_UpdateGamepads();
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGuiViewportDataGlfw
{
	GLFWwindow* Window;
	bool		WindowOwned;
	int			IgnoreWindowPosEventFrame;
	int			IgnoreWindowSizeEventFrame;

	ImGuiViewportDataGlfw()
	{
		Window					   = NULL;
		WindowOwned				   = false;
		IgnoreWindowSizeEventFrame = IgnoreWindowPosEventFrame = -1;
	}
	~ImGuiViewportDataGlfw()
	{
		IM_ASSERT(Window == NULL);
	}
};

static void ImGui_ImplGlfw_WindowCloseCallback(GLFWwindow* window)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
		viewport->PlatformRequestClose = true;
}

// GLFW may dispatch window pos/size events after calling glfwSetWindowPos()/glfwSetWindowSize().
// However: depending on the platform the callback may be invoked at different time:
// - on Windows it appears to be called within the glfwSetWindowPos()/glfwSetWindowSize() call
// - on Linux it is queued and invoked during glfwPollEvents()
// Because the event doesn't always fire on glfwSetWindowXXX() we use a frame counter tag to only
// ignore recent glfwSetWindowXXX() calls.
static void ImGui_ImplGlfw_WindowPosCallback(GLFWwindow* window, int, int)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
	{
		if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
		{
			bool ignore_event = (ImGui::GetFrameCount() <= data->IgnoreWindowPosEventFrame + 1);
			// data->IgnoreWindowPosEventFrame = -1;
			if (ignore_event)
				return;
		}
		viewport->PlatformRequestMove = true;
	}
}

static void ImGui_ImplGlfw_WindowSizeCallback(GLFWwindow* window, int, int)
{
	if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
	{
		if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
		{
			bool ignore_event = (ImGui::GetFrameCount() <= data->IgnoreWindowSizeEventFrame + 1);
			// data->IgnoreWindowSizeEventFrame = -1;
			if (ignore_event)
				return;
		}
		viewport->PlatformRequestResize = true;
	}
}

static void ImGui_ImplGlfw_CreateWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = IM_NEW(ImGuiViewportDataGlfw)();
	viewport->PlatformUserData	= data;

	// GLFW 3.2 unfortunately always set focus on glfwCreateWindow() if GLFW_VISIBLE is set, regardless of GLFW_FOCUSED
	// With GLFW 3.3, the hint GLFW_FOCUS_ON_SHOW fixes this problem
	glfwWindowHint(GLFW_VISIBLE, false);
	glfwWindowHint(GLFW_FOCUSED, false);
	glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
	glfwWindowHint(GLFW_DECORATED, (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? false : true);
	glfwWindowHint(GLFW_FLOATING, (viewport->Flags & ImGuiViewportFlags_TopMost) ? true : false);
	GLFWwindow* share_window = nullptr;
	data->Window			 = glfwCreateWindow((int)viewport->Size.x, (int)viewport->Size.y, "No Title Yet", NULL, share_window);
	data->WindowOwned		 = true;
	viewport->PlatformHandle = (void*)data->Window;
#ifdef _WIN32
	viewport->PlatformHandleRaw = glfwGetWin32Window(data->Window);
#endif
	glfwSetWindowPos(data->Window, (int)viewport->Pos.x, (int)viewport->Pos.y);

	// Install GLFW callbacks for secondary viewports
	glfwSetMouseButtonCallback(data->Window, ImGui_ImplGlfw_MouseButtonCallback);
	glfwSetScrollCallback(data->Window, ImGui_ImplGlfw_ScrollCallback);
	glfwSetKeyCallback(data->Window, ImGui_ImplGlfw_KeyCallback);
	glfwSetCharCallback(data->Window, ImGui_ImplGlfw_CharCallback);
	glfwSetWindowCloseCallback(data->Window, ImGui_ImplGlfw_WindowCloseCallback);
	glfwSetWindowPosCallback(data->Window, ImGui_ImplGlfw_WindowPosCallback);
	glfwSetWindowSizeCallback(data->Window, ImGui_ImplGlfw_WindowSizeCallback);
}

static void ImGui_ImplGlfw_DestroyWindow(ImGuiViewport* viewport)
{
	if (ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData)
	{
		if (data->WindowOwned)
		{
			glfwDestroyWindow(data->Window);
		}
		data->Window = NULL;
		IM_DELETE(data);
	}
	viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

static void ImGui_ImplGlfw_ShowWindow(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;

#if defined(_WIN32)
	// GLFW hack: Hide icon from task bar
	HWND hwnd = (HWND)viewport->PlatformHandleRaw;
	if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
	{
		LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
		ex_style &= ~WS_EX_APPWINDOW;
		ex_style |= WS_EX_TOOLWINDOW;
		::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
	}
#endif

	glfwShowWindow(data->Window);
}

static ImVec2 ImGui_ImplGlfw_GetWindowPos(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	int					   x = 0, y = 0;
	glfwGetWindowPos(data->Window, &x, &y);
	return ImVec2((float)x, (float)y);
}

static void ImGui_ImplGlfw_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
	ImGuiViewportDataGlfw* data		= (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	data->IgnoreWindowPosEventFrame = ImGui::GetFrameCount();
	glfwSetWindowPos(data->Window, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplGlfw_GetWindowSize(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	int					   w = 0, h = 0;
	glfwGetWindowSize(data->Window, &w, &h);
	return ImVec2((float)w, (float)h);
}

static void ImGui_ImplGlfw_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataGlfw* data		 = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	data->IgnoreWindowSizeEventFrame = ImGui::GetFrameCount();
	glfwSetWindowSize(data->Window, (int)size.x, (int)size.y);
}

static void ImGui_ImplGlfw_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwSetWindowTitle(data->Window, title);
}

static void ImGui_ImplGlfw_SetWindowFocus(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwFocusWindow(data->Window);
}

static bool ImGui_ImplGlfw_GetWindowFocus(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	return glfwGetWindowAttrib(data->Window, GLFW_FOCUSED) != 0;
}

static bool ImGui_ImplGlfw_GetWindowMinimized(ImGuiViewport* viewport)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	return glfwGetWindowAttrib(data->Window, GLFW_ICONIFIED) != 0;
}

static void ImGui_ImplGlfw_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
	glfwSetWindowOpacity(data->Window, alpha);
}

static void ImGui_ImplGlfw_RenderWindow(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
}

static void ImGui_ImplGlfw_SwapBuffers(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataGlfw* data = (ImGuiViewportDataGlfw*)viewport->PlatformUserData;
}

//--------------------------------------------------------------------------------------------------------
// IME (Input Method Editor) basic support for e.g. Asian language users
//--------------------------------------------------------------------------------------------------------

// We provide a Win32 implementation because this is such a common issue for IME users
#if defined(_WIN32) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS) && !defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS) && !defined(__GNUC__)
#define HAS_WIN32_IME 1
#include <imm.h>
#ifdef _MSC_VER
#pragma comment(lib, "imm32")
#endif
static void ImGui_ImplWin32_SetImeInputPos(ImGuiViewport* viewport, ImVec2 pos)
{
	COMPOSITIONFORM cf = {CFS_FORCE_POSITION, {(LONG)(pos.x - viewport->Pos.x), (LONG)(pos.y - viewport->Pos.y)}, {0, 0, 0, 0}};
	if (HWND hwnd = (HWND)viewport->PlatformHandleRaw)
		if (HIMC himc = ::ImmGetContext(hwnd))
		{
			::ImmSetCompositionWindow(himc, &cf);
			::ImmReleaseContext(hwnd, himc);
		}
}
#else
#define HAS_WIN32_IME 0
#endif

static void ImGui_ImplGlfw_InitPlatformInterface()
{
	// Register platform interface (will be coupled with a renderer interface)
	ImGuiPlatformIO& platform_io			= ImGui::GetPlatformIO();
	platform_io.Platform_CreateWindow		= ImGui_ImplGlfw_CreateWindow;
	platform_io.Platform_DestroyWindow		= ImGui_ImplGlfw_DestroyWindow;
	platform_io.Platform_ShowWindow			= ImGui_ImplGlfw_ShowWindow;
	platform_io.Platform_SetWindowPos		= ImGui_ImplGlfw_SetWindowPos;
	platform_io.Platform_GetWindowPos		= ImGui_ImplGlfw_GetWindowPos;
	platform_io.Platform_SetWindowSize		= ImGui_ImplGlfw_SetWindowSize;
	platform_io.Platform_GetWindowSize		= ImGui_ImplGlfw_GetWindowSize;
	platform_io.Platform_SetWindowFocus		= ImGui_ImplGlfw_SetWindowFocus;
	platform_io.Platform_GetWindowFocus		= ImGui_ImplGlfw_GetWindowFocus;
	platform_io.Platform_GetWindowMinimized = ImGui_ImplGlfw_GetWindowMinimized;
	platform_io.Platform_SetWindowTitle		= ImGui_ImplGlfw_SetWindowTitle;
	platform_io.Platform_RenderWindow		= ImGui_ImplGlfw_RenderWindow;
	platform_io.Platform_SwapBuffers		= ImGui_ImplGlfw_SwapBuffers;
	platform_io.Platform_SetWindowAlpha		= ImGui_ImplGlfw_SetWindowAlpha;
//#if GLFW_HAS_VULKAN
//    platform_io.Platform_CreateVkSurface = ImGui_ImplGlfw_CreateVkSurface;
//#endif
#if HAS_WIN32_IME
	platform_io.Platform_SetImeInputPos = ImGui_ImplWin32_SetImeInputPos;
#endif

	// Register main window handle (which is owned by the main application, not by us)
	// This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
	ImGuiViewport*		   main_viewport = ImGui::GetMainViewport();
	ImGuiViewportDataGlfw* data			 = IM_NEW(ImGuiViewportDataGlfw)();
	data->Window						 = g_Window;
	data->WindowOwned					 = false;
	main_viewport->PlatformUserData		 = data;
	main_viewport->PlatformHandle		 = (void*)g_Window;
}

static void ImGui_ImplGlfw_ShutdownPlatformInterface() {}
