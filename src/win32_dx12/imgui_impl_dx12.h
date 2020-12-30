#pragma once
#include "imgui.h"

#include <D3D12MemAlloc.h>

enum DXGI_FORMAT;
struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

static constexpr int NUM_FRAMES_IN_FLIGHT = 2;

template<typename T>
inline void SafeRelease(T*& res)
{
	if (res)
	{
		res->Release();
	}
	res = NULL;
}

struct gui_shared_state
{
	ID3D12Device*		device;
	D3D12MA::Allocator* allocator;
	const DXGI_FORMAT	rtv_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	void reset()
	{
		device	  = NULL;
		allocator = NULL;
	}

	void RenderDrawData(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx);
};

struct gui_impl
{
	bool Init(gui_shared_state& shared);
	void Shutdown(gui_shared_state& shared);
	void NewFrame(gui_shared_state& shared, DXGI_FORMAT rtv_format);
	void DestroyDeviceObjects(gui_shared_state& shared);
	bool CreateDeviceObjects(gui_shared_state& shared, DXGI_FORMAT rtv_format);

	void set_descriptor_state();
};
