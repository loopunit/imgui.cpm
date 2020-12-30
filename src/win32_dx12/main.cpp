#include "../imgui_app_fw_impl.h"

#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <array>
#include <limits>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lParam);

gui_shared_state g_shared_state;

struct gui_primary_context
{
	struct FrameContext
	{
		ID3D12CommandAllocator* m_command_allocator;
		UINT64					m_fence_value;
	};

	// Data
	FrameContext				m_frame_contexts[NUM_FRAMES_IN_FLIGHT]				  = {};
	UINT						m_frame_context_index								  = 0;
	IDXGIAdapter*				m_dxgi_adapter										  = NULL;
	ID3D12DescriptorHeap*		m_rtv_desc_heap										  = NULL;
	ID3D12CommandQueue*			m_command_queue										  = NULL;
	ID3D12GraphicsCommandList*	m_command_list										  = NULL;
	ID3D12Fence*				m_fences[NUM_FRAMES_IN_FLIGHT]						  = {};
	HANDLE						m_fence_event										  = NULL;
	UINT64						m_fence_last_signaled_value							  = 0;
	int							m_fence_last_signaled								  = 0;
	IDXGISwapChain3*			m_swap_chain										  = NULL;
	HANDLE						m_swap_chain_waitable								  = NULL;
	ID3D12Resource*				m_main_render_target_resource[NUM_FRAMES_IN_FLIGHT]	  = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_main_render_target_descriptor[NUM_FRAMES_IN_FLIGHT] = {};

	void end_frame(ImVec4 clear_color)
	{
		// Rendering
		FrameContext* frame_context		= wait_for_next_frame_resources();
		UINT		  back_buffer_index = m_swap_chain->GetCurrentBackBufferIndex();
		frame_context->m_command_allocator->Reset();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type				   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags				   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource   = m_main_render_target_resource[back_buffer_index];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

		m_command_list->Reset(frame_context->m_command_allocator, NULL);
		m_command_list->ResourceBarrier(1, &barrier);
		m_command_list->ClearRenderTargetView(m_main_render_target_descriptor[back_buffer_index], (float*)&clear_color, 0, NULL);
		m_command_list->OMSetRenderTargets(1, &m_main_render_target_descriptor[back_buffer_index], FALSE, NULL);
		ImGui::Render();
		g_shared_state.RenderDrawData(ImGui::GetDrawData(), m_command_list);
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
		m_command_list->ResourceBarrier(1, &barrier);
		m_command_list->Close();

		m_command_queue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_command_list);

		ImGuiIO& io = ImGui::GetIO();

		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault(NULL, (void*)m_command_list);
		}

		// swap_chain->Present(1, 0); // Present with vsync
		m_swap_chain->Present(0, 0); // Present without vsync

		UINT64 fence_value = m_fence_last_signaled_value + 1;
		// command_queue->Signal(fences[back_buffer_index], fence_value % (1 >> 31));
		m_command_queue->Signal(m_fences[back_buffer_index], fence_value);
		m_fence_last_signaled_value	 = fence_value;
		m_fence_last_signaled		 = back_buffer_index;
		frame_context->m_fence_value = fence_value;
	}

	// Forward declarations of helper functions
	bool		  create_device(HWND hwnd);
	void		  cleanup_device();
	void		  create_render_target();
	void		  cleanup_render_target();
	void		  wait_for_last_submitted_frame();
	FrameContext* wait_for_next_frame_resources();
	void		  resize_swap_chain(HWND hwnd, int width, int height);

	static inline gui_primary_context& instance()
	{
		static gui_primary_context self;
		return self;
	}
};

struct gui_os_globals
{
	bool		m_ready = false;
	WNDCLASSEXA m_wc;
	HWND		m_hwnd;

	std::array<int, 2> m_position;
	std::array<int, 2> m_size;

	static inline gui_os_globals& instance()
	{
		static gui_os_globals self;
		return self;
	}

	void init_window(const std::array<int, 2>& p, const std::array<int, 2>& s)
	{
		m_position = p;
		m_size	   = s;

		// Create application window
		ImGui_ImplWin32_EnableDpiAwareness();
		m_wc = {
			sizeof(WNDCLASSEXA),
			CS_CLASSDC,
			[](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT {
				if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
				{
					return true;
				}

				switch (msg)
				{
				case WM_SIZE:
					if (g_shared_state.device != NULL && wparam != SIZE_MINIMIZED && gui_os_globals::instance().m_ready)
					{
						gui_primary_context::instance().wait_for_last_submitted_frame();
						// Don't need to do this (unless device changes or resets?) ImGui_ImplDX12_InvalidateDeviceObjects();
						gui_primary_context::instance().cleanup_render_target();
						gui_primary_context::instance().resize_swap_chain(hwnd, (UINT)LOWORD(lparam), (UINT)HIWORD(lparam));
						gui_primary_context::instance().create_render_target();
						// Don't need to do this (unless device changes or resets?) ImGui_ImplDX12_CreateDeviceObjects();
					}
					return 0;
				case WM_SYSCOMMAND:
					if ((wparam & 0xfff0) == SC_KEYMENU)
					{
						// Disable ALT application menu
						return 0;
					}
					break;
				case WM_DESTROY:
					::PostQuitMessage(0);
					return 0;
				}
				return ::DefWindowProc(hwnd, msg, wparam, lparam);
			},
			0L,
			0L,
			GetModuleHandleA(NULL),
			NULL,
			NULL,
			NULL,
			NULL,
			_T("ImGui Example"),
			NULL};

		::RegisterClassExA(&m_wc);

		m_hwnd = ::CreateWindowA(
			m_wc.lpszClassName, _T("Dear ImGui DirectX12 Example"), WS_OVERLAPPEDWINDOW, m_position[0], m_position[1], m_size[0], m_size[1], NULL, NULL, m_wc.hInstance, NULL);
	}

	void destroy_window()
	{
		::DestroyWindow(m_hwnd);
		::UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
	}

	void show_window()
	{
		::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(m_hwnd);
	}
};

gui_impl g_gui_impl;

void set_window_title_win32_dx12(const char* title)
{
	::SetWindowTextA(gui_os_globals::instance().m_hwnd, title);
}

bool init_gui_win32_dx12()
{
	gui_os_globals::instance().init_window({100, 100}, {1280, 800});

	if (!gui_primary_context::instance().create_device(gui_os_globals::instance().m_hwnd))
	{
		gui_primary_context::instance().cleanup_device();
		gui_os_globals::instance().destroy_window();
		return false;
	}

	gui_os_globals::instance().show_window();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
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
	ImGui_ImplWin32_Init(gui_os_globals::instance().m_hwnd);

	g_gui_impl.Init(g_shared_state);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
	// ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	// io.Fonts->AddFontDefault();
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	// IM_ASSERT(font != NULL);

	gui_os_globals::instance().m_ready = true;

	return true;
}

bool pump_gui_win32_dx12()
{
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}
		return true;
	}
	return false;
}

void begin_frame_gui_win32_dx12()
{
	// Start the Dear ImGui frame
	g_gui_impl.NewFrame(g_shared_state, g_shared_state.rtv_format);
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void end_frame_gui_win32_dx12(ImVec4 clear_color)
{
	gui_primary_context::instance().end_frame(clear_color);
}

void destroy_gui_win32_dx12()
{
	gui_primary_context::instance().wait_for_last_submitted_frame();
	g_gui_impl.Shutdown(g_shared_state);
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	gui_primary_context::instance().cleanup_device();
	gui_os_globals::instance().destroy_window();
}

bool gui_primary_context::create_device(HWND hwnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC1 sd;
	{
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount		  = NUM_FRAMES_IN_FLIGHT;
		sd.Width			  = 0;
		sd.Height			  = 0;
		sd.Format			  = g_shared_state.rtv_format;
		sd.Flags			  = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		sd.BufferUsage		  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count	  = 1;
		sd.SampleDesc.Quality = 0;
		sd.SwapEffect		  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.AlphaMode		  = DXGI_ALPHA_MODE_UNSPECIFIED;
		sd.Scaling			  = DXGI_SCALING_STRETCH;
		sd.Stereo			  = FALSE;
	}

#ifdef DX12_ENABLE_DEBUG_LAYER
	ID3D12Debug* dx12_debug = NULL;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dx12_debug))))
	{
		dx12_debug->EnableDebugLayer();
		dx12_debug->Release();
	}
#endif

	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	if (D3D12CreateDevice(NULL, feature_level, IID_PPV_ARGS(&g_shared_state.device)) != S_OK)
	{
		return false;
	}

	{
		IDXGIFactory4* dxgi_factory = NULL;
		if (::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)) != S_OK)
		{
			return false;
		}

		if (dxgi_factory->EnumAdapterByLuid(g_shared_state.device->GetAdapterLuid(), IID_PPV_ARGS(&m_dxgi_adapter)) != S_OK)
		{
			return false;
		}

		dxgi_factory->Release();
	}

	{
		D3D12MA::ALLOCATOR_DESC allocator_desc = {};
		allocator_desc.pDevice				   = g_shared_state.device;
		allocator_desc.pAdapter				   = m_dxgi_adapter;
		// g_allocationCallbacks.pAllocate		  = &CustomAllocate;
		// g_allocationCallbacks.pFree			  = &CustomFree;
		// g_allocationCallbacks.pUserData		  = CUSTOM_ALLOCATION_USER_DATA;
		// desc.pallocationCallbacks			  = &g_allocationCallbacks;

		if (D3D12MA::CreateAllocator(&allocator_desc, &g_shared_state.allocator) != S_OK)
		{
			return false;
		}

		switch (g_shared_state.allocator->GetD3D12Options().ResourceHeapTier)
		{
		case D3D12_RESOURCE_HEAP_TIER_1:
			OutputDebugStringA("ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_1\n");
			break;
		case D3D12_RESOURCE_HEAP_TIER_2:
			OutputDebugStringA("ResourceHeapTier = D3D12_RESOURCE_HEAP_TIER_2\n");
			break;
		default:
			assert(0);
		}
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors				= NUM_FRAMES_IN_FLIGHT;
		desc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask					= 1;
		if (g_shared_state.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtv_desc_heap)) != S_OK)
		{
			return false;
		}

		SIZE_T						rtv_descriptor_size = g_shared_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle			= m_rtv_desc_heap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			m_main_render_target_descriptor[i] = rtv_handle;
			rtv_handle.ptr += rtv_descriptor_size;
		}
	}

	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type					  = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Flags					  = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask				  = 1;
		if (g_shared_state.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)) != S_OK)
		{
			return false;
		}
	}

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		if (g_shared_state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frame_contexts[i].m_command_allocator)) != S_OK)
		{
			return false;
		}
	}

	if (g_shared_state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frame_contexts[0].m_command_allocator, NULL, IID_PPV_ARGS(&m_command_list)) != S_OK ||
		m_command_list->Close() != S_OK)
	{
		return false;
	}

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		if (g_shared_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fences[i])) != S_OK)
		{
			return false;
		}
	}

	m_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_fence_event == NULL)
	{
		return false;
	}

	{
		IDXGIFactory4*	 dxgi_factory = NULL;
		IDXGISwapChain1* swap_chain	  = NULL;
		if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)) != S_OK || dxgi_factory->CreateSwapChainForHwnd(m_command_queue, hwnd, &sd, NULL, NULL, &swap_chain) != S_OK ||
			swap_chain->QueryInterface(IID_PPV_ARGS(&m_swap_chain)) != S_OK)
		{
			return false;
		}
		swap_chain->Release();
		dxgi_factory->Release();
		m_swap_chain->SetMaximumFrameLatency(NUM_FRAMES_IN_FLIGHT);
		m_swap_chain_waitable = m_swap_chain->GetFrameLatencyWaitableObject();
	}

	create_render_target();
	return true;
}

void gui_primary_context::cleanup_device()
{
	cleanup_render_target();

	if (m_swap_chain)
	{
		m_swap_chain->Release();
		m_swap_chain = NULL;
	}

	if (m_swap_chain_waitable != NULL)
	{
		CloseHandle(m_swap_chain_waitable);
	}

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		if (m_frame_contexts[i].m_command_allocator)
		{
			m_frame_contexts[i].m_command_allocator->Release();
			m_frame_contexts[i].m_command_allocator = NULL;
		}
	}

	if (m_command_queue)
	{
		m_command_queue->Release();
		m_command_queue = NULL;
	}

	if (m_command_list)
	{
		m_command_list->Release();
		m_command_list = NULL;
	}

	if (m_rtv_desc_heap)
	{
		m_rtv_desc_heap->Release();
		m_rtv_desc_heap = NULL;
	}

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		if (m_fences[i])
		{
			m_fences[i]->Release();
			m_fences[i] = NULL;
		}
	}

	if (m_fence_event)
	{
		CloseHandle(m_fence_event);
		m_fence_event = NULL;
	}

	if (g_shared_state.device)
	{
		g_shared_state.device->Release();
		g_shared_state.device = NULL;
	}

	if (m_dxgi_adapter)
	{
		m_dxgi_adapter->Release();
		m_dxgi_adapter = NULL;
	}

#ifdef DX12_ENABLE_DEBUG_LAYER
	IDXGIDebug1* dxgi_debug = NULL;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
	{
		dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
		dxgi_debug->Release();
	}
#endif
}

void gui_primary_context::create_render_target()
{
	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		ID3D12Resource* back_buffer = NULL;
		m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
		g_shared_state.device->CreateRenderTargetView(back_buffer, NULL, m_main_render_target_descriptor[i]);
		m_main_render_target_resource[i] = back_buffer;
	}
}

void gui_primary_context::cleanup_render_target()
{
	wait_for_last_submitted_frame();

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		if (m_main_render_target_resource[i])
		{
			m_main_render_target_resource[i]->Release();
			m_main_render_target_resource[i] = NULL;
		}
	}
}

void gui_primary_context::wait_for_last_submitted_frame()
{
	FrameContext* frame_context = &m_frame_contexts[m_frame_context_index % NUM_FRAMES_IN_FLIGHT];

	UINT64 fence_value = frame_context->m_fence_value;
	if (fence_value == 0)
	{
		// no fence was signaled
		return;
	}

	frame_context->m_fence_value = 0;
	if (m_fences[m_fence_last_signaled]->GetCompletedValue() >= fence_value)
	{
		return;
	}

	m_fences[m_fence_last_signaled]->SetEventOnCompletion(fence_value, m_fence_event);
	WaitForSingleObject(m_fence_event, INFINITE);
}

gui_primary_context::FrameContext* gui_primary_context::wait_for_next_frame_resources()
{
	UINT next_frame_index = m_frame_context_index + 1;
	m_frame_context_index = next_frame_index;

	HANDLE waitable_objects[]	= {m_swap_chain_waitable, NULL};
	DWORD  num_waitable_objects = 1;

	FrameContext* frame_context = &m_frame_contexts[next_frame_index % NUM_FRAMES_IN_FLIGHT];
	UINT64		  fence_value	= frame_context->m_fence_value;

	if (fence_value != 0)
	{
		// no fence was signaled
		frame_context->m_fence_value = 0;
		m_fences[m_fence_last_signaled]->SetEventOnCompletion(fence_value, m_fence_event);
		waitable_objects[1] = m_fence_event;
		num_waitable_objects += 1;
	}

	WaitForMultipleObjects(num_waitable_objects, waitable_objects, TRUE, INFINITE);

	return frame_context;
}

void gui_primary_context::resize_swap_chain(HWND hwnd, int width, int height)
{
	DXGI_SWAP_CHAIN_DESC1 sd;
	m_swap_chain->GetDesc1(&sd);
	sd.Width  = width;
	sd.Height = height;

	IDXGIFactory4* dxgi_factory = NULL;
	m_swap_chain->GetParent(IID_PPV_ARGS(&dxgi_factory));

	m_swap_chain->Release();
	CloseHandle(m_swap_chain_waitable);

	IDXGISwapChain1* swap_chain = NULL;
	dxgi_factory->CreateSwapChainForHwnd(m_command_queue, hwnd, &sd, NULL, NULL, &swap_chain);
	swap_chain->QueryInterface(IID_PPV_ARGS(&m_swap_chain));
	swap_chain->Release();
	dxgi_factory->Release();

	m_swap_chain->SetMaximumFrameLatency(NUM_FRAMES_IN_FLIGHT);

	m_swap_chain_waitable = m_swap_chain->GetFrameLatencyWaitableObject();
	assert(m_swap_chain_waitable != NULL);
}
