// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include "ImguiCommon.h"

namespace FG
{
	//
	// Imgui Renderer
	//

	class ImguiRenderer
	{
		// variables
	private:
		ImageID		_fontTexture;
		SamplerID	_fontSampler;
		GPipelineID _pipeline;

		BufferID _vertexBuffer;
		BufferID _indexBuffer;
		BufferID _uniformBuffer;

		BytesU _vertexBufSize;
		BytesU _indexBufSize;

		PipelineResources _resources;

		// methods
	public:
		ImguiRenderer();
		~ImguiRenderer();

		bool Initialize(ImGuiContext* ctx, const FrameGraph& fg);
		void Deinitialize(const FrameGraph& fg);

		Task Draw(ImDrawData* draw_data, ImGuiContext* ctx, const CommandBuffer& cmdbuf, LogicalPassID passId, ArrayView<Task> dependencies = Default);

	private:
		bool _CreatePipeline(const FrameGraph&);
		bool _CreateSampler(const FrameGraph&);

		ND_ Task _RecreateBuffers(ImDrawData* draw_data, ImGuiContext* _context, const CommandBuffer&);
		ND_ Task _CreateFontTexture(ImGuiContext* _context, const CommandBuffer&);
		ND_ Task _UpdateUniformBuffer(ImDrawData* draw_data, ImGuiContext* _context, const CommandBuffer&);
	};

} // namespace FG
