// dear imgui: Renderer for DirectX12
// This needs to be used along with a Platform Binding (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'D3D12_GPU_DESCRIPTOR_HANDLE' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Multi-viewport support. Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.
//      FIXME: The transition from removing a viewport and moving the window in an existing hosted viewport tends to flicker.
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.
// Missing features, issues:
//  [ ] 64-bit only for now! (Because sizeof(ImTextureId) == sizeof(void*)). See github.com/ocornut/imgui/pull/301

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "imgui.h"
#include "imgui_impl_dx12.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

#include <D3D12MemAlloc.h>

// Forward Declarations
static void ImGui_ImplDX12_InitPlatformInterface();
static void ImGui_ImplDX12_ShutdownPlatformInterface();

// Buffers used during the rendering of a frame
struct ImGui_ImplDX12_RenderBuffers
{
	ID3D12Resource*		 m_index_buffer;
	ID3D12Resource*		 m_vertex_buffer;
	D3D12MA::Allocation* m_index_buffer_alloc;
	D3D12MA::Allocation* m_vertex_buffer_alloc;
	int					 m_index_buffer_size;
	int					 m_vertex_buffer_size;

	void destroy()
	{
		SafeRelease(m_index_buffer);
		SafeRelease(m_vertex_buffer);
		SafeRelease(m_index_buffer_alloc);
		SafeRelease(m_vertex_buffer_alloc);
		m_index_buffer_size = m_vertex_buffer_size = 0;
	}
};

// Buffers used for secondary viewports created by the multi-viewports systems
struct ImGui_ImplDX12_FrameContext
{
	ID3D12CommandAllocator*		m_command_allocator;
	ID3D12Resource*				m_render_target;
	D3D12_CPU_DESCRIPTOR_HANDLE m_render_target_cpu_descriptors;
};

// Helper structure we store in the void* RendererUserData field of each ImGuiViewport to easily retrieve our backend data.
// Main viewport created by application will only use the Resources field.
// Secondary viewports created by this back-end will use all the fields (including Window fields),
struct ImGuiViewportDataDx12
{
	// Window
	ID3D12CommandQueue*			 m_command_queue;
	ID3D12GraphicsCommandList*	 m_command_list;
	ID3D12DescriptorHeap*		 m_rtv_desc_heap;
	IDXGISwapChain3*			 m_swap_chain;
	ID3D12Fence*				 m_fence;
	UINT64						 m_fence_signaled_value;
	HANDLE						 m_fence_event;
	ImGui_ImplDX12_FrameContext* m_frame_context;

	// Render buffers
	UINT						  m_frame_index;
	ImGui_ImplDX12_RenderBuffers* m_frame_render_buffers;

	ImGuiViewportDataDx12()
	{
		m_command_queue		   = NULL;
		m_command_list		   = NULL;
		m_rtv_desc_heap		   = NULL;
		m_swap_chain		   = NULL;
		m_fence				   = NULL;
		m_fence_signaled_value = 0;
		m_fence_event		   = NULL;
		m_frame_context		   = new ImGui_ImplDX12_FrameContext[NUM_FRAMES_IN_FLIGHT];
		m_frame_index		   = UINT_MAX;
		m_frame_render_buffers = new ImGui_ImplDX12_RenderBuffers[NUM_FRAMES_IN_FLIGHT];

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			m_frame_context[i].m_command_allocator = NULL;
			m_frame_context[i].m_render_target	   = NULL;

			// Create buffers with a default size (they will later be grown as needed)
			m_frame_render_buffers[i].m_index_buffer		= NULL;
			m_frame_render_buffers[i].m_vertex_buffer		= NULL;
			m_frame_render_buffers[i].m_index_buffer_alloc	= NULL;
			m_frame_render_buffers[i].m_vertex_buffer_alloc = NULL;
			m_frame_render_buffers[i].m_vertex_buffer_size	= 5000;
			m_frame_render_buffers[i].m_index_buffer_size	= 10000;
		}
	}
	~ImGuiViewportDataDx12()
	{
		IM_ASSERT(m_command_queue == NULL && m_command_list == NULL);
		IM_ASSERT(m_rtv_desc_heap == NULL);
		IM_ASSERT(m_swap_chain == NULL);
		IM_ASSERT(m_fence == NULL);
		IM_ASSERT(m_fence_event == NULL);

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			IM_ASSERT(m_frame_context[i].m_command_allocator == NULL && m_frame_context[i].m_render_target == NULL);
			IM_ASSERT(m_frame_render_buffers[i].m_index_buffer == NULL && m_frame_render_buffers[i].m_vertex_buffer == NULL);
			IM_ASSERT(m_frame_render_buffers[i].m_index_buffer_alloc == NULL && m_frame_render_buffers[i].m_vertex_buffer_alloc == NULL);
		}

		delete[] m_frame_context;
		m_frame_context = NULL;
		delete[] m_frame_render_buffers;
		m_frame_render_buffers = NULL;
	}
};

//

bool gui_impl::Init(gui_shared_state& shared)
{
	// Setup back-end capabilities flags
	ImGuiIO& io			   = ImGui::GetIO();
	io.BackendRendererName = "imgui_impl_dx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional) // FIXME-VIEWPORT: Actually unfinished..

	// Create a dummy ImGuiViewportDataDx12 holder for the main viewport,
	// Since this is created and managed by the application, we will only use the ->Resources[] fields.
	ImGuiViewport* main_viewport	= ImGui::GetMainViewport();
	main_viewport->RendererUserData = IM_NEW(ImGuiViewportDataDx12)();

	// Setup back-end capabilities flags
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui_ImplDX12_InitPlatformInterface();
	}

	return true;
}

void gui_impl::Shutdown(gui_shared_state& shared)
{
	// Manually delete main viewport render resources in-case we haven't initialized for viewports
	ImGuiViewport* main_viewport = ImGui::GetMainViewport();
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)main_viewport->RendererUserData)
	{
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			data->m_frame_render_buffers[i].destroy();
		}
		IM_DELETE(data);
		main_viewport->RendererUserData = NULL;
	}

	// Clean up windows and device objects
	ImGui_ImplDX12_ShutdownPlatformInterface();
	DestroyDeviceObjects(shared);

	shared.reset();
}

struct gui_render_state
{
	ID3D12RootSignature*		m_root_signature			  = NULL;
	ID3D12PipelineState*		m_pipeline_state			  = NULL;
	ID3D12Resource*				m_font_texture_resource		  = NULL;
	D3D12MA::Allocation*		m_font_texture_resource_alloc = NULL;
	ID3D12DescriptorHeap*		cbv_srv_heap				  = NULL;
	D3D12_CPU_DESCRIPTOR_HANDLE g_font_srv_cpu_desc_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE g_font_srv_gpu_desc_handle;

	void CreateFontsTexture(gui_shared_state& shared)
	{
		// Build texture atlas
		ImGuiIO&	   io = ImGui::GetIO();
		unsigned char* pixels;
		int			   width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		// Upload texture to graphics system
		{
			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType				 = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC desc;

			ZeroMemory(&desc, sizeof(desc));
			desc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment			= 0;
			desc.Width				= width;
			desc.Height				= height;
			desc.DepthOrArraySize	= 1;
			desc.MipLevels			= 1;
			desc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count	= 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

			ID3D12Resource*		 tex	   = NULL;
			D3D12MA::Allocation* tex_alloc = NULL;
			shared.allocator->CreateResource(&allocation_desc, &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &tex_alloc, IID_PPV_ARGS(&tex));

			UINT upload_pitch		= (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
			UINT upload_size		= height * upload_pitch;
			desc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment			= 0;
			desc.Width				= upload_size;
			desc.Height				= 1;
			desc.DepthOrArraySize	= 1;
			desc.MipLevels			= 1;
			desc.Format				= DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count	= 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags				= D3D12_RESOURCE_FLAG_NONE;

			allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

			ID3D12Resource*		 upload_buffer		 = NULL;
			D3D12MA::Allocation* upload_buffer_alloc = NULL;
			HRESULT hr = shared.allocator->CreateResource(&allocation_desc, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &upload_buffer_alloc, IID_PPV_ARGS(&upload_buffer));
			IM_ASSERT(SUCCEEDED(hr));

			void*		mapped = NULL;
			D3D12_RANGE range  = {0, upload_size};
			hr				   = upload_buffer->Map(0, &range, &mapped);
			IM_ASSERT(SUCCEEDED(hr));
			for (int y = 0; y < height; y++)
			{
				memcpy((void*)((uintptr_t)mapped + y * upload_pitch), pixels + y * width * 4, width * 4);
			}
			upload_buffer->Unmap(0, &range);

			D3D12_TEXTURE_COPY_LOCATION src_location		= {};
			src_location.pResource							= upload_buffer;
			src_location.Type								= D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src_location.PlacedFootprint.Footprint.Format	= DXGI_FORMAT_R8G8B8A8_UNORM;
			src_location.PlacedFootprint.Footprint.Width	= width;
			src_location.PlacedFootprint.Footprint.Height	= height;
			src_location.PlacedFootprint.Footprint.Depth	= 1;
			src_location.PlacedFootprint.Footprint.RowPitch = upload_pitch;

			D3D12_TEXTURE_COPY_LOCATION dst_location = {};
			dst_location.pResource					 = tex;
			dst_location.Type						 = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst_location.SubresourceIndex			 = 0;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type				   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags				   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource   = tex;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			ID3D12Fence* fence = NULL;
			hr				   = shared.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
			IM_ASSERT(SUCCEEDED(hr));

			HANDLE event = CreateEvent(0, 0, 0, 0);
			IM_ASSERT(event != NULL);

			D3D12_COMMAND_QUEUE_DESC queue_desc = {};
			queue_desc.Type						= D3D12_COMMAND_LIST_TYPE_DIRECT;
			queue_desc.Flags					= D3D12_COMMAND_QUEUE_FLAG_NONE;
			queue_desc.NodeMask					= 1;

			ID3D12CommandQueue* cmd_queue = NULL;
			hr							  = shared.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&cmd_queue));
			IM_ASSERT(SUCCEEDED(hr));

			ID3D12CommandAllocator* cmd_alloc = NULL;
			hr								  = shared.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_alloc));
			IM_ASSERT(SUCCEEDED(hr));

			ID3D12GraphicsCommandList* cmd_list = NULL;
			hr									= shared.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_alloc, NULL, IID_PPV_ARGS(&cmd_list));
			IM_ASSERT(SUCCEEDED(hr));

			cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, NULL);
			cmd_list->ResourceBarrier(1, &barrier);

			hr = cmd_list->Close();
			IM_ASSERT(SUCCEEDED(hr));

			cmd_queue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmd_list);
			hr = cmd_queue->Signal(fence, 1);
			IM_ASSERT(SUCCEEDED(hr));

			fence->SetEventOnCompletion(1, event);
			WaitForSingleObject(event, INFINITE);

			cmd_list->Release();
			cmd_alloc->Release();
			cmd_queue->Release();
			CloseHandle(event);
			fence->Release();
			upload_buffer_alloc->Release();
			upload_buffer->Release();

			// Create texture view
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
			ZeroMemory(&srv_desc, sizeof(srv_desc));
			srv_desc.Format					   = DXGI_FORMAT_R8G8B8A8_UNORM;
			srv_desc.ViewDimension			   = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels	   = desc.MipLevels;
			srv_desc.Texture2D.MostDetailedMip = 0;
			srv_desc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shared.device->CreateShaderResourceView(tex, &srv_desc, g_font_srv_cpu_desc_handle);
			SafeRelease(m_font_texture_resource);
			SafeRelease(m_font_texture_resource_alloc);
			m_font_texture_resource		  = tex;
			m_font_texture_resource_alloc = tex_alloc;
		}

		// Store our identifier
		static_assert(sizeof(ImTextureID) >= sizeof(g_font_srv_gpu_desc_handle.ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
		io.Fonts->TexID = (ImTextureID)g_font_srv_gpu_desc_handle.ptr;
	}

	bool CreateDeviceObjects(gui_shared_state& shared, DXGI_FORMAT rtv_format)
	{
		if (!shared.device)
		{
			return false;
		}

		if (m_pipeline_state)
		{
			DestroyDeviceObjects(shared);
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors				= 1;
			desc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (shared.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&cbv_srv_heap)) != S_OK)
			{
				return false;
			}

			g_font_srv_cpu_desc_handle = cbv_srv_heap->GetCPUDescriptorHandleForHeapStart();
			g_font_srv_gpu_desc_handle = cbv_srv_heap->GetGPUDescriptorHandleForHeapStart();
		}

		// Create the root signature
		{
			D3D12_DESCRIPTOR_RANGE desc_range			 = {};
			desc_range.RangeType						 = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			desc_range.NumDescriptors					 = 1;
			desc_range.BaseShaderRegister				 = 0;
			desc_range.RegisterSpace					 = 0;
			desc_range.OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER param[2] = {};

			param[0].ParameterType			  = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			param[0].Constants.ShaderRegister = 0;
			param[0].Constants.RegisterSpace  = 0;
			param[0].Constants.Num32BitValues = 16;
			param[0].ShaderVisibility		  = D3D12_SHADER_VISIBILITY_VERTEX;

			param[1].ParameterType						 = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param[1].DescriptorTable.NumDescriptorRanges = 1;
			param[1].DescriptorTable.pDescriptorRanges	 = &desc_range;
			param[1].ShaderVisibility					 = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_STATIC_SAMPLER_DESC static_sampler = {};
			static_sampler.Filter					 = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			static_sampler.AddressU					 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_sampler.AddressV					 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_sampler.AddressW					 = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_sampler.MipLODBias				 = 0.f;
			static_sampler.MaxAnisotropy			 = 0;
			static_sampler.ComparisonFunc			 = D3D12_COMPARISON_FUNC_ALWAYS;
			static_sampler.BorderColor				 = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			static_sampler.MinLOD					 = 0.f;
			static_sampler.MaxLOD					 = 0.f;
			static_sampler.ShaderRegister			 = 0;
			static_sampler.RegisterSpace			 = 0;
			static_sampler.ShaderVisibility			 = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC desc = {};
			desc.NumParameters			   = _countof(param);
			desc.pParameters			   = param;
			desc.NumStaticSamplers		   = 1;
			desc.pStaticSamplers		   = &static_sampler;
			desc.Flags					   = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
						 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			ID3DBlob* blob = NULL;
			if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) != S_OK)
			{
				return false;
			}

			shared.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_root_signature));
			blob->Release();
		}

		// By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
		// If you would like to use this DX12 sample code but remove this dependency you can:
		//  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
		//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
		// See https://github.com/ocornut/imgui/pull/638 for sources and details.

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
		memset(&pso_desc, 0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		pso_desc.NodeMask			   = 1;
		pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso_desc.pRootSignature		   = m_root_signature;
		pso_desc.SampleMask			   = UINT_MAX;
		pso_desc.NumRenderTargets	   = 1;
		pso_desc.RTVFormats[0]		   = rtv_format;
		pso_desc.SampleDesc.Count	   = 1;
		pso_desc.Flags				   = D3D12_PIPELINE_STATE_FLAG_NONE;

		ID3DBlob* vertex_shader_blob;
		ID3DBlob* pixel_shader_blob;

		// Create the vertex shader
		{
			static const char* vertex_shader =
				"cbuffer vertexBuffer : register(b0) \
				{\
				  float4x4 ProjectionMatrix; \
				};\
				struct VS_INPUT\
				{\
				  float2 pos : POSITION;\
				  float4 col : COLOR0;\
				  float2 uv  : TEXCOORD0;\
				};\
				\
				struct PS_INPUT\
				{\
				  float4 pos : SV_POSITION;\
				  float4 col : COLOR0;\
				  float2 uv  : TEXCOORD0;\
				};\
				\
				PS_INPUT main(VS_INPUT input)\
				{\
				  PS_INPUT output;\
				  output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
				  output.col = input.col;\
				  output.uv  = input.uv;\
				  return output;\
				}";

			if (FAILED(D3DCompile(vertex_shader, strlen(vertex_shader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &vertex_shader_blob, NULL)))
			{
				return false; // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the
							  // blob!
			}
			pso_desc.VS = {vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize()};

			// Create the input layout
			static D3D12_INPUT_ELEMENT_DESC local_layout[] = {
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)IM_OFFSETOF(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			pso_desc.InputLayout = {local_layout, 3};
		}

		// Create the pixel shader
		{
			static const char* pixel_shader =
				"struct PS_INPUT\
				{\
				  float4 pos : SV_POSITION;\
				  float4 col : COLOR0;\
				  float2 uv  : TEXCOORD0;\
				};\
				SamplerState sampler0 : register(s0);\
				Texture2D texture0 : register(t0);\
				\
				float4 main(PS_INPUT input) : SV_Target\
				{\
				  float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
				  return out_col; \
				}";

			if (FAILED(D3DCompile(pixel_shader, strlen(pixel_shader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &pixel_shader_blob, NULL)))
			{
				vertex_shader_blob->Release();
				return false; // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the
							  // blob!
			}
			pso_desc.PS = {pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize()};
		}

		// Create the blending setup
		{
			D3D12_BLEND_DESC& desc					   = pso_desc.BlendState;
			desc.AlphaToCoverageEnable				   = false;
			desc.RenderTarget[0].BlendEnable		   = true;
			desc.RenderTarget[0].SrcBlend			   = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend			   = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp			   = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha		   = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].DestBlendAlpha		   = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha		   = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}

		// Create the rasterizer state
		{
			D3D12_RASTERIZER_DESC& desc = pso_desc.RasterizerState;
			desc.FillMode				= D3D12_FILL_MODE_SOLID;
			desc.CullMode				= D3D12_CULL_MODE_NONE;
			desc.FrontCounterClockwise	= FALSE;
			desc.DepthBias				= D3D12_DEFAULT_DEPTH_BIAS;
			desc.DepthBiasClamp			= D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
			desc.SlopeScaledDepthBias	= D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			desc.DepthClipEnable		= true;
			desc.MultisampleEnable		= FALSE;
			desc.AntialiasedLineEnable	= FALSE;
			desc.ForcedSampleCount		= 0;
			desc.ConservativeRaster		= D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		}

		// Create depth-stencil State
		{
			D3D12_DEPTH_STENCIL_DESC& desc = pso_desc.DepthStencilState;
			desc.DepthEnable			   = false;
			desc.DepthWriteMask			   = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc				   = D3D12_COMPARISON_FUNC_ALWAYS;
			desc.StencilEnable			   = false;
			desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc																		= D3D12_COMPARISON_FUNC_ALWAYS;
			desc.BackFace																					= desc.FrontFace;
		}

		HRESULT result_pipeline_state = shared.device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
		vertex_shader_blob->Release();
		pixel_shader_blob->Release();

		if (result_pipeline_state != S_OK)
		{
			return false;
		}

		CreateFontsTexture(shared);

		return true;
	}

	void DestroyDeviceObjects(gui_shared_state& shared)
	{
		if (!shared.device)
		{
			return;
		}

		SafeRelease(m_root_signature);
		SafeRelease(m_pipeline_state);
		SafeRelease(m_font_texture_resource_alloc);
		SafeRelease(m_font_texture_resource);

		ImGuiIO& io		= ImGui::GetIO();
		io.Fonts->TexID = NULL; // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.

		if (cbv_srv_heap)
		{
			cbv_srv_heap->Release();
			cbv_srv_heap = NULL;
		}
	}
};

extern gui_impl	 g_gui_impl;
gui_render_state g_render_state;

void gui_impl::DestroyDeviceObjects(gui_shared_state& shared)
{
	g_render_state.DestroyDeviceObjects(shared);
}

bool gui_impl::CreateDeviceObjects(gui_shared_state& shared, DXGI_FORMAT rtv_format)
{
	return g_render_state.CreateDeviceObjects(shared, rtv_format);
}

struct VERTEX_CONSTANT_BUFFER
{
	float mvp[4][4];
};

static void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, ImGui_ImplDX12_RenderBuffers* fr)
{
	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	VERTEX_CONSTANT_BUFFER vertex_constant_buffer;
	{
		float L			= draw_data->DisplayPos.x;
		float R			= draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T			= draw_data->DisplayPos.y;
		float B			= draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float mvp[4][4] = {
			{2.0f / (R - L), 0.0f, 0.0f, 0.0f},
			{0.0f, 2.0f / (T - B), 0.0f, 0.0f},
			{0.0f, 0.0f, 0.5f, 0.0f},
			{(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
		};
		memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
	}

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width	= draw_data->DisplaySize.x;
	vp.Height	= draw_data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	ctx->RSSetViewports(1, &vp);

	// Bind shader and vertex buffers
	unsigned int			 stride = sizeof(ImDrawVert);
	unsigned int			 offset = 0;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	vbv.BufferLocation = fr->m_vertex_buffer->GetGPUVirtualAddress() + offset;
	vbv.SizeInBytes	   = fr->m_vertex_buffer_size * stride;
	vbv.StrideInBytes  = stride;
	ctx->IASetVertexBuffers(0, 1, &vbv);
	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = fr->m_index_buffer->GetGPUVirtualAddress();
	ibv.SizeInBytes	   = fr->m_index_buffer_size * sizeof(ImDrawIdx);
	ibv.Format		   = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ctx->IASetIndexBuffer(&ibv);
	ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->SetPipelineState(g_render_state.m_pipeline_state);
	ctx->SetGraphicsRootSignature(g_render_state.m_root_signature);
	ctx->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);

	// Setup blend factor
	const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
	ctx->OMSetBlendFactor(blend_factor);
}

void gui_impl::NewFrame(gui_shared_state& shared, DXGI_FORMAT rtv_format)
{
	if (!g_render_state.m_pipeline_state)
	{
		CreateDeviceObjects(shared, rtv_format);
	}
}

void gui_shared_state::RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx)
{
	ctx->SetDescriptorHeaps(1, &g_render_state.cbv_srv_heap);

	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	ImGuiViewportDataDx12* render_data = (ImGuiViewportDataDx12*)draw_data->OwnerViewport->RendererUserData;
	render_data->m_frame_index++;
	ImGui_ImplDX12_RenderBuffers* fr = &render_data->m_frame_render_buffers[render_data->m_frame_index % NUM_FRAMES_IN_FLIGHT];

	// Create and grow vertex/index buffers if needed
	if (fr->m_vertex_buffer == NULL || fr->m_vertex_buffer_size < draw_data->TotalVtxCount)
	{
		SafeRelease(fr->m_vertex_buffer);
		SafeRelease(fr->m_vertex_buffer_alloc);
		fr->m_vertex_buffer_size = draw_data->TotalVtxCount + 5000;

		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension		  = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width			  = fr->m_vertex_buffer_size * sizeof(ImDrawVert);
		desc.Height			  = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels		  = 1;
		desc.Format			  = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout			  = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags			  = D3D12_RESOURCE_FLAG_NONE;

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType				 = D3D12_HEAP_TYPE_UPLOAD;

		if (allocator->CreateResource(&allocation_desc, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &fr->m_vertex_buffer_alloc, IID_PPV_ARGS(&fr->m_vertex_buffer)) < 0)
		{
			return;
		}
	}
	if (fr->m_index_buffer == NULL || fr->m_index_buffer_size < draw_data->TotalIdxCount)
	{
		SafeRelease(fr->m_index_buffer);
		SafeRelease(fr->m_index_buffer_alloc);
		fr->m_index_buffer_size = draw_data->TotalIdxCount + 10000;

		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension		  = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width			  = fr->m_index_buffer_size * sizeof(ImDrawIdx);
		desc.Height			  = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels		  = 1;
		desc.Format			  = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout			  = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags			  = D3D12_RESOURCE_FLAG_NONE;

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType				 = D3D12_HEAP_TYPE_UPLOAD;

		if (allocator->CreateResource(&allocation_desc, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &fr->m_index_buffer_alloc, IID_PPV_ARGS(&fr->m_index_buffer)) < 0)
		{
			return;
		}
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	void *		vtx_resource, *idx_resource;
	D3D12_RANGE range;
	memset(&range, 0, sizeof(D3D12_RANGE));

	if (fr->m_vertex_buffer->Map(0, &range, &vtx_resource) != S_OK)
	{
		return;
	}

	if (fr->m_index_buffer->Map(0, &range, &idx_resource) != S_OK)
	{
		return;
	}

	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx*	idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	fr->m_vertex_buffer->Unmap(0, &range);
	fr->m_index_buffer->Unmap(0, &range);

	// Setup desired DX state
	ImGui_ImplDX12_SetupRenderState(draw_data, ctx, fr);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int	   global_vtx_offset = 0;
	int	   global_idx_offset = 0;
	ImVec2 clip_off			 = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_index = 0; cmd_index < cmd_list->CmdBuffer.Size; cmd_index++)
		{
			const ImDrawCmd* cmd = &cmd_list->CmdBuffer[cmd_index];
			if (cmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (cmd->UserCallback == ImDrawCallback_ResetRenderState)
				{
					ImGui_ImplDX12_SetupRenderState(draw_data, ctx, fr);
				}
				else
				{
					cmd->UserCallback(cmd_list, cmd);
				}
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = {
					(LONG)(cmd->ClipRect.x - clip_off.x), (LONG)(cmd->ClipRect.y - clip_off.y), (LONG)(cmd->ClipRect.z - clip_off.x), (LONG)(cmd->ClipRect.w - clip_off.y)};
				ctx->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&cmd->TextureId);
				ctx->RSSetScissorRects(1, &r);
				ctx->DrawIndexedInstanced(cmd->ElemCount, 1, cmd->IdxOffset + global_idx_offset, cmd->VtxOffset + global_vtx_offset, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

extern gui_shared_state g_shared_state;

static void imgui_create_secondary_window(ImGuiViewport* viewport)
{
	ImGuiViewportDataDx12* data = IM_NEW(ImGuiViewportDataDx12)();
	viewport->RendererUserData	= data;

	// PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
	// Some back-ends will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND.
	HWND hwnd = viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);

	data->m_frame_index = UINT_MAX;

	// Create command queue.
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags					= D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type						= D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT res = S_OK;
	res			= g_shared_state.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&data->m_command_queue));
	IM_ASSERT(res == S_OK);

	// Create command allocator.
	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
	{
		res = g_shared_state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&data->m_frame_context[i].m_command_allocator));
		IM_ASSERT(res == S_OK);
	}

	// Create command list.
	res = g_shared_state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, data->m_frame_context[0].m_command_allocator, NULL, IID_PPV_ARGS(&data->m_command_list));
	IM_ASSERT(res == S_OK);
	data->m_command_list->Close();

	// Create fence.
	res = g_shared_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&data->m_fence));
	IM_ASSERT(res == S_OK);

	data->m_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	IM_ASSERT(data->m_fence_event != NULL);

	// Create swap chain
	// FIXME-VIEWPORT: May want to copy/inherit swap chain settings from the user/application.
	DXGI_SWAP_CHAIN_DESC1 sd1;
	ZeroMemory(&sd1, sizeof(sd1));
	sd1.BufferCount		   = NUM_FRAMES_IN_FLIGHT;
	sd1.Width			   = (UINT)viewport->Size.x;
	sd1.Height			   = (UINT)viewport->Size.y;
	sd1.Format			   = g_shared_state.rtv_format;
	sd1.BufferUsage		   = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd1.SampleDesc.Count   = 1;
	sd1.SampleDesc.Quality = 0;
	sd1.SwapEffect		   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd1.AlphaMode		   = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd1.Scaling			   = DXGI_SCALING_STRETCH;
	sd1.Stereo			   = FALSE;

	IDXGIFactory4* dxgi_factory = NULL;
	res							= ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
	IM_ASSERT(res == S_OK);

	IDXGISwapChain1* swap_chain = NULL;
	res							= dxgi_factory->CreateSwapChainForHwnd(data->m_command_queue, hwnd, &sd1, NULL, NULL, &swap_chain);
	IM_ASSERT(res == S_OK);

	dxgi_factory->Release();

	// Or swapChain.As(&mSwapChain)
	IM_ASSERT(data->m_swap_chain == NULL);
	swap_chain->QueryInterface(IID_PPV_ARGS(&data->m_swap_chain));
	swap_chain->Release();

	// Create the render targets
	if (data->m_swap_chain)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors				= NUM_FRAMES_IN_FLIGHT;
		desc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask					= 1;

		HRESULT hr = g_shared_state.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&data->m_rtv_desc_heap));
		IM_ASSERT(hr == S_OK);

		SIZE_T						rtv_descriptor_size = g_shared_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle			= data->m_rtv_desc_heap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			data->m_frame_context[i].m_render_target_cpu_descriptors = rtv_handle;
			rtv_handle.ptr += rtv_descriptor_size;
		}

		ID3D12Resource* back_buffer;
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			IM_ASSERT(data->m_frame_context[i].m_render_target == NULL);
			data->m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
			g_shared_state.device->CreateRenderTargetView(back_buffer, NULL, data->m_frame_context[i].m_render_target_cpu_descriptors);
			data->m_frame_context[i].m_render_target = back_buffer;
		}
	}

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		data->m_frame_render_buffers[i].destroy();
	}
}

static void imgui_wait_for_pending_operations(ImGuiViewportDataDx12* data)
{
	HRESULT hr = S_FALSE;
	if (data && data->m_command_queue && data->m_fence && data->m_fence_event)
	{
		hr = data->m_command_queue->Signal(data->m_fence, ++data->m_fence_signaled_value);
		IM_ASSERT(hr == S_OK);
		::WaitForSingleObject(data->m_fence_event, 0); // Reset any forgotten waits
		hr = data->m_fence->SetEventOnCompletion(data->m_fence_signaled_value, data->m_fence_event);
		IM_ASSERT(hr == S_OK);
		::WaitForSingleObject(data->m_fence_event, INFINITE);
	}
}

static void imgui_destroy_secondary_window(ImGuiViewport* viewport)
{
	// The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
	if (ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData)
	{
		imgui_wait_for_pending_operations(data);

		SafeRelease(data->m_command_queue);
		SafeRelease(data->m_command_list);
		SafeRelease(data->m_swap_chain);
		SafeRelease(data->m_rtv_desc_heap);
		SafeRelease(data->m_fence);
		::CloseHandle(data->m_fence_event);
		data->m_fence_event = NULL;

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			SafeRelease(data->m_frame_context[i].m_render_target);
			SafeRelease(data->m_frame_context[i].m_command_allocator);
			data->m_frame_render_buffers[i].destroy();
		}
		IM_DELETE(data);
	}
	viewport->RendererUserData = NULL;
}

static void imgui_set_secondary_window_size(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	imgui_wait_for_pending_operations(data);

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		SafeRelease(data->m_frame_context[i].m_render_target);
	}

	if (data->m_swap_chain)
	{
		ID3D12Resource* back_buffer = NULL;
		data->m_swap_chain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			data->m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
			g_shared_state.device->CreateRenderTargetView(back_buffer, NULL, data->m_frame_context[i].m_render_target_cpu_descriptors);
			data->m_frame_context[i].m_render_target = back_buffer;
		}
	}
}

static void imgui_render_secondary_window(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	ImGui_ImplDX12_FrameContext* frame_context	 = &data->m_frame_context[data->m_frame_index % NUM_FRAMES_IN_FLIGHT];
	UINT						 back_buffer_idx = data->m_swap_chain->GetCurrentBackBufferIndex();

	const ImVec4		   clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	D3D12_RESOURCE_BARRIER barrier	   = {};
	barrier.Type					   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	   = data->m_frame_context[back_buffer_idx].m_render_target;
	barrier.Transition.Subresource	   = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore	   = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	   = D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Draw
	ID3D12GraphicsCommandList* cmd_list = data->m_command_list;

	frame_context->m_command_allocator->Reset();
	cmd_list->Reset(frame_context->m_command_allocator, NULL);
	cmd_list->ResourceBarrier(1, &barrier);
	cmd_list->OMSetRenderTargets(1, &data->m_frame_context[back_buffer_idx].m_render_target_cpu_descriptors, FALSE, NULL);
	if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
	{
		cmd_list->ClearRenderTargetView(data->m_frame_context[back_buffer_idx].m_render_target_cpu_descriptors, (float*)&clear_color, 0, NULL);
	}

	g_shared_state.RenderDrawData(viewport->DrawData, cmd_list);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
	cmd_list->ResourceBarrier(1, &barrier);
	cmd_list->Close();

	data->m_command_queue->Wait(data->m_fence, data->m_fence_signaled_value);
	data->m_command_queue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmd_list);
	data->m_command_queue->Signal(data->m_fence, ++data->m_fence_signaled_value);
}

static void imgui_swap_secondary_window_buffers(ImGuiViewport* viewport, void*)
{
	ImGuiViewportDataDx12* data = (ImGuiViewportDataDx12*)viewport->RendererUserData;

	data->m_swap_chain->Present(0, 0);
	while (data->m_fence->GetCompletedValue() < data->m_fence_signaled_value)
	{
		::SwitchToThread();
	}
}

void ImGui_ImplDX12_InitPlatformInterface()
{
	ImGuiPlatformIO& platform_io	   = ImGui::GetPlatformIO();
	platform_io.Renderer_CreateWindow  = imgui_create_secondary_window;
	platform_io.Renderer_DestroyWindow = imgui_destroy_secondary_window;
	platform_io.Renderer_SetWindowSize = imgui_set_secondary_window_size;
	platform_io.Renderer_RenderWindow  = imgui_render_secondary_window;
	platform_io.Renderer_SwapBuffers   = imgui_swap_secondary_window_buffers;
}

void ImGui_ImplDX12_ShutdownPlatformInterface()
{
	ImGui::DestroyPlatformWindows();
}
