#define NOMINMAX

#include "../imgui_app_fw_impl.h"

#include "VulkanDevice2.h"
#include <Framework/Vulkan/VulkanSwapchain.h>
#include <framegraph/FG.h>
#include <framegraph/Shared/EnumUtils.h>
#include <pipeline_compiler/VPipelineCompiler.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <limits>
#include <memory>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

#if defined(_WIN32) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS) && !defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS) && !defined(__GNUC__)
#define HAS_WIN32_IME 1
#include <imm.h>
#ifdef _MSC_VER
#pragma comment(lib, "imm32")
#endif
#else
#define HAS_WIN32_IME 0
#endif

#include <basisu_transcoder.h>

#include <fstream>
#include <filesystem>
#include <map>

namespace FG
{
	class IntermImage final : public std::enable_shared_from_this<IntermImage>
	{
		// types
	public:
		struct Level
		{
			uint3		   dimension;
			EPixelFormat   format = Default;
			ImageLayer	   layer  = 0_layer;
			MipmapLevel	   mipmap = 0_mipmap;
			BytesU		   rowPitch;
			BytesU		   slicePitch;
			Array<uint8_t> pixels;
		};

		using ArrayLayers_t = Array<Level>; // size == 1 for non-array images
		using Mipmaps_t		= Array<ArrayLayers_t>;

		// variables
	private:
		String _srcPath;

		Mipmaps_t _data; // mipmaps[] { layers[] { level } }
		EImage	  _imageType = Default;

		bool _immutable = false;

		// methods
	public:
		IntermImage() {}
		explicit IntermImage(const ImageView& view)
		{
			_imageType = view.Dimension().z > 1 ? EImage_3D : view.Dimension().y > 1 ? EImage_2D : EImage_1D;

			_data.resize(1);
			_data[0].resize(1);

			auto& level		 = _data[0][0];
			level.dimension	 = Max(1u, view.Dimension());
			level.format	 = view.Format();
			level.layer		 = 0_layer;
			level.mipmap	 = 0_mipmap;
			level.rowPitch	 = view.RowPitch();
			level.slicePitch = view.SlicePitch();
			level.pixels.resize(size_t(level.slicePitch * level.dimension.z));

			BytesU offset = 0_b;
			for (auto& part : view.Parts())
			{
				BytesU size = ArraySizeOf(part);
				std::memcpy(level.pixels.data() + offset, part.data(), size_t(size));
				offset += size;
			}

			ASSERT(offset == ArraySizeOf(level.pixels));
			STATIC_ASSERT(sizeof(level.pixels[0]) == sizeof(view.Parts()[0][0]));
		}

		explicit IntermImage(StringView path) : _srcPath{path} {}
		IntermImage(Mipmaps_t&& data, EImage type, StringView path = Default) : _srcPath{path}, _data{std::move(data)}, _imageType{type} {}

		void MakeImmutable()
		{
			_immutable = true;
		}

		void ReleaseData()
		{
			Mipmaps_t temp;
			std::swap(temp, _data);
		}

		void SetData(Mipmaps_t&& data, EImage type)
		{
			ASSERT(not _immutable);
			_data	   = std::move(data);
			_imageType = type;
		}

		ND_ StringView GetPath() const
		{
			return _srcPath;
		}

		ND_ bool IsImmutable() const
		{
			return _immutable;
		}

		ND_ Mipmaps_t const& GetData() const
		{
			return _data;
		}

		ND_ EImage GetType() const
		{
			return _imageType;
		}

		ND_ EImageDim GetImageDim() const
		{
			switch (_imageType)
			{
			case EImage_1D:
			case EImage_1DArray:
				return EImageDim_1D;
			case EImage_2D:
			case EImage_2DArray:
			case EImage_Cube:
			case EImage_CubeArray:
				return EImageDim_2D;
			case EImage_3D:
				return EImageDim_3D;
			}
			return Default;
		}
	};
} // namespace FG

struct basis_cache
{
	struct basis_texture
	{
		basist::basisu_image_info info;
		basist::basisu_file_info  file_info;

		basist::transcoder_texture_format format;
		uint32_t						  block_width;
		uint32_t						  block_height;
		uint32_t						  bytes_per_block;

		struct level
		{
			uint32_t					 width;
			uint32_t					 height;
			uint32_t					 blocks;
			std::unique_ptr<std::byte[]> data;
		};
		std::vector<std::unique_ptr<level>> image_levels;
	};

	std::unique_ptr<basist::etc1_global_selector_codebook> m_basis_codebook;
	std::map<std::wstring, std::unique_ptr<basis_texture>> m_basis_cache;
	std::map<std::wstring, FG::ImageID>					   m_texture_cache;

	basis_cache()
	{
		m_basis_codebook.reset(new basist::etc1_global_selector_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb));
	}

	~basis_cache()
	{
		m_basis_cache.clear();
	}

	void cache_basis_texture(std::wstring cache_key, uint32_t file_size, std::unique_ptr<std::byte[]> file_mem, const basist::transcoder_texture_format dest_format)
	{
		if (basist::basisu_transcoder transcoder(m_basis_codebook.get()); transcoder.validate_header(file_mem.get(), file_size))
		{
			auto new_texture			 = std::make_unique<basis_texture>();
			new_texture->format			 = dest_format;
			new_texture->bytes_per_block = basist::basis_get_bytes_per_block_or_pixel(dest_format);
			new_texture->block_width	 = basist::basis_get_block_width(dest_format);
			new_texture->block_height	 = basist::basis_get_block_height(dest_format);

			if (transcoder.get_image_info(file_mem.get(), file_size, new_texture->info, 0))
			{
				transcoder.get_file_info(file_mem.get(), file_size, new_texture->file_info);

				if (transcoder.start_transcoding(file_mem.get(), file_size))
				{
					for (uint32_t level = 0; level < new_texture->info.m_total_levels; ++level)
					{
						auto new_level = std::make_unique<basis_texture::level>();

						if (transcoder.get_image_level_desc(file_mem.get(), file_size, 0, level, new_level->width, new_level->height, new_level->blocks))
						{
							new_level->data.reset(new std::byte[new_level->blocks * new_texture->bytes_per_block]);
							transcoder.transcode_image_level(file_mem.get(), file_size, 0, level, new_level->data.get(), new_level->blocks, dest_format);
							new_texture->image_levels.emplace_back(std::move(new_level));
						}
						else
						{
							// TODO: error!
						}
					}

					m_basis_cache.emplace(std::pair<std::wstring, std::unique_ptr<basis_texture>>(std::move(cache_key), std::move(new_texture)));
					transcoder.stop_transcoding();
				}
				else
				{
					// TODO: error!
				}
			}
			else
			{
				// TODO: error!
			}
		}
		else
		{
			// TODO: error!
		}
	}

	void cache_basis_texture(const std::filesystem::path& p, const basist::transcoder_texture_format dest_format)
	{
		if (std::filesystem::exists(p))
		{
			if (auto file_size = std::filesystem::file_size(p); file_size > 0)
			{
				auto native_path = p.native();
				if (std::FILE* f = _wfopen(native_path.c_str(), L"rb"); f != nullptr)
				{
					auto file_mem = std::unique_ptr<std::byte[]>(new std::byte[file_size]);
					if (auto read_size = std::fread(file_mem.get(), 1, file_size, f); read_size == file_size)
					{
						cache_basis_texture(std::move(native_path), static_cast<uint32_t>(file_size), std::move(file_mem), dest_format);
					}
					else
					{
						// TODO: error!
					}
					std::fclose(f);
				}
				else
				{
					// TODO: error!
				}
			}
			else
			{
				// TODO: error!
			}
		}
		else
		{
			// TODO: error!
		}
	}

// clang-format off
	#define BASIS_FG_PAIR( _visit_ ) \
		_visit_( basist::transcoder_texture_format::cTFBC1_RGB,			FG::EPixelFormat::BC1_RGB8_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFBC3_RGBA,		FG::EPixelFormat::BC3_RGBA8_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFBC4_R,			FG::EPixelFormat::BC4_R8_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFBC5_RG,			FG::EPixelFormat::BC5_RG8_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFBC7_RGBA,		FG::EPixelFormat::BC7_RGBA8_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFETC2_EAC_R11,	FG::EPixelFormat::EAC_R11_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFETC2_EAC_RG11,	FG::EPixelFormat::EAC_RG11_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFASTC_4x4_RGBA,	FG::EPixelFormat::ASTC_RGBA_4x4 ) \
		_visit_( basist::transcoder_texture_format::cTFRGBA32,			FG::EPixelFormat::RGBA32U ) \
		_visit_( basist::transcoder_texture_format::cTFRGB565,			FG::EPixelFormat::RGB_5_6_5_UNorm ) \
		_visit_( basist::transcoder_texture_format::cTFRGBA4444,		FG::EPixelFormat::RGBA4_UNorm )
	// clang-format on

	inline std::optional<FG::EPixelFormat> convert_format(const basist::transcoder_texture_format fmt)
	{
		switch (fmt)
		{
			// clang-format off
#define BASIS_TO_FG_VISITOR(_basis_, _fg_fmt_)	\
		case _basis_:							\
			return _fg_fmt_;					\
/**/
		BASIS_FG_PAIR(BASIS_TO_FG_VISITOR)
#undef BASIS_TO_FG_VISITOR
			// clang-format on
		}
		return std::nullopt;
	}

	std::optional<FG::Task> load_texture_from_cache(const std::wstring& cache_key, const FG::CommandBuffer& cmdbuf)
	{
		if (auto itor = m_basis_cache.find(cache_key); itor != m_basis_cache.end())
		{
			if (auto& tex = std::get<1>(*itor); auto fg_format = convert_format(tex->format))
			{
				const bool	   is_cube		= false;
				const auto	   mipmap_count = static_cast<FG::uint>(tex->image_levels.size());
				const FG::uint array_layers(1);
				FG::uint3	   dim		= {tex->image_levels[0]->width, tex->image_levels[0]->height, 1};
				FG::EImage	   img_type = FG::EImage_2D;

				auto& fg_info{FG::EPixelFormat_GetInfo(*fg_format)};
				auto  new_img = cmdbuf->GetFrameGraph()->CreateImage(
					 FG::ImageDesc{}.SetDimension(dim).SetFormat(FG::EPixelFormat::RGBA8_UNorm).SetUsage(FG::EImageUsage::Sampled | FG::EImageUsage::TransferDst), FG::Default);

				FG::Task curr_task = nullptr;

				for (FG::uint lvl = 0; lvl < tex->image_levels.size(); ++lvl)
				{
					FG::ArrayView<uint8_t> data_view{(uint8_t*)tex->image_levels[lvl]->data.get(), tex->image_levels[lvl]->blocks * tex->bytes_per_block};

					FGC::BytesU bytes_pitch = static_cast<FGC::BytesU>(tex->image_levels[lvl]->width / tex->block_width * tex->bytes_per_block);

					curr_task = cmdbuf->AddTask(
						FG::UpdateImage{}
							.SetImage(new_img, {0, 0, 0}, FG::MipmapLevel(lvl))
							.SetData(data_view, FGC::uint3{FGC::uint(tex->image_levels[lvl]->width), FGC::uint(tex->image_levels[lvl]->height), FGC::uint(0)}, bytes_pitch)
							.DependsOn(curr_task));
				}

				m_texture_cache.emplace(std::pair<std::wstring, FG::ImageID>(cache_key, std::move(new_img)));
				return curr_task;
			}
			else
			{
				// TODO: error!
			}
		}

		return std::nullopt;
	}

	std::optional<FG::ImageID> acquire_texture(const std::wstring& cache_key, const FG::CommandBuffer& cmdbuf)
	{
		if (auto itor = m_texture_cache.find(cache_key); itor != m_texture_cache.end() && cmdbuf->GetFrameGraph()->IsResourceAlive(itor->second))
		{
			return cmdbuf->GetFrameGraph()->AcquireResource(itor->second);
		}
		return std::nullopt;
	}

	void release_texture(FG::ImageID& img, const FG::CommandBuffer& cmdbuf)
	{
		cmdbuf->GetFrameGraph()->ReleaseResource(img);
	}
};

struct imgui_renderer_window
{
	FG::BufferID m_vertex_buffer;
	FG::BufferID m_index_buffer;
	FG::BufferID m_uniform_buffer;

	FG::BytesU m_vertex_buf_size;
	FG::BytesU m_index_buf_size;

	FG::PipelineResources m_resources;

	std::map<ImTextureID, FG::ImageID> m_texture_cache;
};

struct imgui_renderer
{
	FG::ImageID		m_font_texture;
	FG::SamplerID	m_font_sampler;
	FG::GPipelineID m_pipeline;

	bool init_shared(ImGuiContext* _context, const FG::FrameGraph& fg)
	{
		CHECK_ERR(create_pipeline(fg));
		CHECK_ERR(create_sampler(fg));

		// initialize font atlas
		{
			uint8_t* pixels;
			int		 width, height;
			_context->IO.Fonts->GetTexDataAsRGBA32(OUT & pixels, OUT & width, OUT & height);
		}
		return true;
	}

	bool init(imgui_renderer_window& pw, ImGuiContext* _context, const FG::FrameGraph& fg)
	{
		CHECK_ERR(init_pipeline(pw, fg));
		return true;
	}

	void destroy(imgui_renderer_window& pw, const FG::FrameGraph& fg)
	{
		if (fg)
		{
			fg->ReleaseResource(INOUT pw.m_vertex_buffer);
			fg->ReleaseResource(INOUT pw.m_index_buffer);
			fg->ReleaseResource(INOUT pw.m_uniform_buffer);
		}
	}

	void destroy_shared(const FG::FrameGraph& fg)
	{
		if (fg)
		{
			fg->ReleaseResource(INOUT m_font_texture);
			fg->ReleaseResource(INOUT m_font_sampler);
			fg->ReleaseResource(INOUT m_pipeline);
		}
	}

	template<typename T_USERDRAW_HANDLER>
	FG::Task draw(
		imgui_renderer_window& pw, ImDrawData* draw_data, ImGuiContext* _context, const FG::CommandBuffer& cmdbuf, FG::LogicalPassID pass_id, FG::ArrayView<FG::Task> dependencies,
		T_USERDRAW_HANDLER userdraw_handler = [](const ImDrawList& cmd_list, const ImDrawCmd& cmd) -> FG::Task { return nullptr; })
	{
		CHECK_ERR(cmdbuf and _context);

		ASSERT(draw_data->TotalVtxCount > 0);

		int fb_width  = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

		if (fb_width <= 0 || fb_height <= 0)
		{
			return nullptr;
		}

		FG::SubmitRenderPass submit{pass_id};

		submit.DependsOn(create_buffers(pw, draw_data, _context, cmdbuf));
		submit.DependsOn(update_uniform_buffer(pw, draw_data, _context, cmdbuf));

		for (auto dep : dependencies)
		{
			submit.DependsOn(dep);
		}

		FG::VertexInputState vert_input;
		vert_input.Bind(FG::VertexBufferID(), FG::SizeOf<ImDrawVert>);
		vert_input.Add(FG::VertexID("aPos"), FG::EVertexType::Float2, FG::OffsetOf(&ImDrawVert::pos));
		vert_input.Add(FG::VertexID("aUV"), FG::EVertexType::Float2, FG::OffsetOf(&ImDrawVert::uv));
		vert_input.Add(FG::VertexID("aColor"), FG::EVertexType::UByte4_Norm, FG::OffsetOf(&ImDrawVert::col));

		FG::uint idx_offset = 0;
		FG::uint vtx_offset = 0;

		ImVec2 clip_off	  = draw_data->DisplayPos;		 // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		pw.m_resources.BindBuffer(FG::UniformID("uPushConstant"), pw.m_uniform_buffer);

		for (int i = 0; i < draw_data->CmdListsCount; ++i)
		{
			const ImDrawList& cmd_list = *draw_data->CmdLists[i];

			for (int j = 0; j < cmd_list.CmdBuffer.Size; ++j)
			{
				const ImDrawCmd& cmd = cmd_list.CmdBuffer[j];

				if (cmd.UserCallback)
				{
					if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
					{
						// state is bound to the draw tasks, so this isn't needed?
					}
					else
					{
						submit.DependsOn(userdraw_handler(cmd_list, cmd));
					}
				}
				else
				{
					FG::RectI scissor;
					scissor.left   = int((cmd.ClipRect.x - clip_off.x) * clip_scale.x);
					scissor.top	   = int((cmd.ClipRect.y - clip_off.y) * clip_scale.y);
					scissor.right  = int((cmd.ClipRect.z - clip_off.x) * clip_scale.x);
					scissor.bottom = int((cmd.ClipRect.w - clip_off.y) * clip_scale.y);

					if (scissor.left < fb_width && scissor.top < fb_height && scissor.right >= 0.0f && scissor.bottom >= 0.0f)
					{
						// Negative offsets are illegal for vkCmdSetScissor
						if (scissor.left < 0)
						{
							scissor.left = 0;
						}

						if (scissor.top < 0)
						{
							scissor.top = 0;
						}

						if (cmd.TextureId)
						{
							// pw.m_resources.BindTexture(FG::UniformID("sTexture"), static_cast<FG::RawImageID>(cmd.TextureId), m_font_sampler);
							pw.m_resources.BindTexture(FG::UniformID("sTexture"), m_font_texture, m_font_sampler);
						}
						else
						{
							pw.m_resources.BindTexture(FG::UniformID("sTexture"), m_font_texture, m_font_sampler);
						}

						cmdbuf->AddTask(
							pass_id, FG::DrawIndexed{}
										 .SetPipeline(m_pipeline)
										 .AddResources(FG::DescriptorSetID{"0"}, pw.m_resources)
										 .AddVertexBuffer(FG::VertexBufferID(), pw.m_vertex_buffer)
										 .SetVertexInput(vert_input)
										 .SetTopology(FG::EPrimitive::TriangleList)
										 .SetIndexBuffer(pw.m_index_buffer, (FG::BytesU)0, FG::EIndex::UShort)
										 .AddColorBuffer(FG::RenderTargetID::Color_0, FG::EBlendFactor::SrcAlpha, FG::EBlendFactor::OneMinusSrcAlpha, FG::EBlendOp::Add)
										 .SetDepthTestEnabled(false)
										 .SetCullMode(FG::ECullMode::None)
										 .Draw(cmd.ElemCount, 1, idx_offset, int(vtx_offset), 0)
										 .AddScissor(scissor));
					}
				}
				idx_offset += cmd.ElemCount;
			}

			vtx_offset += cmd_list.VtxBuffer.Size;
		}

		return cmdbuf->AddTask(submit);
	}

	bool create_pipeline(const FG::FrameGraph& fg)
	{
		using namespace std::string_literals;

		FG::GraphicsPipelineDesc desc;

		desc.AddShader(FG::EShader::Vertex, FG::EShaderLangFormat::VKSL_100, "main", R"#(
			#version 450 core
			layout(location = 0) in vec2 aPos;
			layout(location = 1) in vec2 aUV;
			layout(location = 2) in vec4 aColor;

			//layout(push_constant) uniform uPushConstant {
			layout(set=0, binding=1, std140) uniform uPushConstant {
				vec2 uScale;
				vec2 uTranslate;
			} pc;

			out gl_PerVertex{
				vec4 gl_Position;
			};

			layout(location = 0) out struct{
				vec4 Color;
				vec2 UV;
			} Out;

			void main()
			{
				Out.Color = aColor;
				Out.UV = aUV;
				gl_Position = vec4(aPos*pc.uScale+pc.uTranslate, 0, 1);
			})#"s);

		desc.AddShader(FG::EShader::Fragment, FG::EShaderLangFormat::VKSL_100, "main", R"#(
			#version 450 core
			layout(location = 0) out vec4 out_Color0;

			layout(set=0, binding=0) uniform sampler2D sTexture;

			layout(location = 0) in struct{
				vec4 Color;
				vec2 UV;
			} In;

			void main()
			{
				out_Color0 = In.Color * texture(sTexture, In.UV.st);
			})#"s);

		m_pipeline = fg->CreatePipeline(desc);
		CHECK_ERR(m_pipeline);
		return true;
	}

	bool init_pipeline(imgui_renderer_window& pw, const FG::FrameGraph& fg)
	{
		CHECK_ERR(fg->InitPipelineResources(m_pipeline, FG::DescriptorSetID("0"), OUT pw.m_resources));
		return true;
	}

	bool create_sampler(const FG::FrameGraph& fg)
	{
		FG::SamplerDesc desc;
		desc.SetFilter(FG::EFilter::Linear, FG::EFilter::Linear, FG::EMipmapFilter::Linear);
		desc.SetAddressMode(FG::EAddressMode::Repeat);
		desc.SetLodRange(-1000.0f, 1000.0f);

		m_font_sampler = fg->CreateSampler(desc);
		CHECK_ERR(m_font_sampler);
		return true;
	}

	ND_ FG::Task create_font_texture(ImGuiContext* _context, const FG::CommandBuffer& cmdbuf)
	{
		if (m_font_texture)
		{
			return null;
		}

		uint8_t* pixels;
		int		 width, height;

		_context->IO.Fonts->GetTexDataAsRGBA32(OUT & pixels, OUT & width, OUT & height);

		size_t upload_size = width * height * 4 * sizeof(char);

		m_font_texture = cmdbuf->GetFrameGraph()->CreateImage(
			FG::ImageDesc{}
				.SetDimension({FG::uint(width), FG::uint(height)})
				.SetFormat(FG::EPixelFormat::RGBA8_UNorm)
				.SetUsage(FG::EImageUsage::Sampled | FG::EImageUsage::TransferDst),
			FG::Default, "UI.FontTexture");
		CHECK_ERR(m_font_texture);

		return cmdbuf->AddTask(FG::UpdateImage{}.SetImage(m_font_texture).SetData(pixels, upload_size, FG::uint2{FG::int2{width, height}}));
	}

	ND_ FG::Task create_buffers(imgui_renderer_window& pw, ImDrawData* draw_data, ImGuiContext* _context, const FG::CommandBuffer& cmdbuf)
	{
		FG::FrameGraph fg		   = cmdbuf->GetFrameGraph();
		FG::BytesU	   vertex_size = draw_data->TotalVtxCount * FG::SizeOf<ImDrawVert>;
		FG::BytesU	   index_size  = draw_data->TotalIdxCount * FG::SizeOf<ImDrawIdx>;

		if (not pw.m_vertex_buffer or vertex_size > pw.m_vertex_buf_size)
		{
			fg->ReleaseResource(INOUT pw.m_vertex_buffer);

			pw.m_vertex_buf_size = vertex_size;
			pw.m_vertex_buffer	 = fg->CreateBuffer(FG::BufferDesc{vertex_size, FG::EBufferUsage::TransferDst | FG::EBufferUsage::Vertex}, FG::Default, "UI.VertexBuffer");
		}

		if (not pw.m_index_buffer or index_size > pw.m_index_buf_size)
		{
			fg->ReleaseResource(INOUT pw.m_index_buffer);

			pw.m_index_buf_size = index_size;
			pw.m_index_buffer	= fg->CreateBuffer(FG::BufferDesc{index_size, FG::EBufferUsage::TransferDst | FG::EBufferUsage::Index}, FG::Default, "UI.IndexBuffer");
		}

		FG::BytesU vb_offset;
		FG::BytesU ib_offset;

		FG::Task last_task;

		for (int i = 0; i < draw_data->CmdListsCount; ++i)
		{
			const ImDrawList& cmd_list = *draw_data->CmdLists[i];

			last_task = cmdbuf->AddTask(FG::UpdateBuffer{}.SetBuffer(pw.m_vertex_buffer).AddData(cmd_list.VtxBuffer.Data, cmd_list.VtxBuffer.Size, vb_offset).DependsOn(last_task));
			last_task = cmdbuf->AddTask(FG::UpdateBuffer{}.SetBuffer(pw.m_index_buffer).AddData(cmd_list.IdxBuffer.Data, cmd_list.IdxBuffer.Size, ib_offset).DependsOn(last_task));

			vb_offset += cmd_list.VtxBuffer.Size * FG::SizeOf<ImDrawVert>;
			ib_offset += cmd_list.IdxBuffer.Size * FG::SizeOf<ImDrawIdx>;
		}

		ASSERT(vertex_size == vb_offset);
		ASSERT(index_size == ib_offset);
		return last_task;
	}

	ND_ FG::Task update_uniform_buffer(imgui_renderer_window& pw, ImDrawData* draw_data, ImGuiContext* _context, const FG::CommandBuffer& cmdbuf)
	{
		if (not pw.m_uniform_buffer)
		{
			pw.m_uniform_buffer =
				cmdbuf->GetFrameGraph()->CreateBuffer(FG::BufferDesc{(FG::BytesU)16, FG::EBufferUsage::Uniform | FG::EBufferUsage::TransferDst}, FG::Default, "UI.UniformBuffer");
			CHECK_ERR(pw.m_uniform_buffer);
		}

		FG::float4 pc_data;
		// scale:
		pc_data[0] = 2.0f / (draw_data->DisplaySize.x * _context->IO.DisplayFramebufferScale.x);
		pc_data[1] = 2.0f / (draw_data->DisplaySize.y * _context->IO.DisplayFramebufferScale.y);
		// transform:
		pc_data[2] = -1.0f - draw_data->DisplayPos.x * pc_data[0];
		pc_data[3] = -1.0f - draw_data->DisplayPos.y * pc_data[1];

		return cmdbuf->AddTask(FG::UpdateBuffer{}.SetBuffer(pw.m_uniform_buffer).AddData(&pc_data, 1));
	}
};

struct platform_window_data
{
	GLFWwindow* m_window;
	bool		m_window_owned;
	int			m_ignore_window_pos_event_frame;
	int			m_ignore_window_size_event_frame;

	platform_window_data()
	{
		m_window						 = NULL;
		m_window_owned					 = false;
		m_ignore_window_size_event_frame = m_ignore_window_pos_event_frame = -1;
	}

	~platform_window_data()
	{
		IM_ASSERT(m_window == NULL);
	}
};

struct platform_device_factory : public FGC::IVulkanSurface
{
	GLFWwindow* m_window;

	platform_device_factory(GLFWwindow* wnd) : m_window{wnd} {}

	virtual ~platform_device_factory() {}

	FGC::ArrayView<const char*> GetRequiredExtensions() const override
	{
		uint32_t	 required_extension_count = 0;
		const char** required_extensions	  = glfwGetRequiredInstanceExtensions(&required_extension_count);

		return FGC::ArrayView<const char*>{required_extensions, required_extension_count};
	}

	SurfaceVk_t Create(InstanceVk_t inst) const override
	{
		VkSurfaceKHR surf = VK_NULL_HANDLE;
		glfwCreateWindowSurface(FGC::BitCast<VkInstance>(inst), m_window, nullptr, &surf);
		return FGC::BitCast<SurfaceVk_t>(surf);
	}
};

struct platform_renderer_data
{
	bool m_is_primary{false};

	FGC::VulkanDevice2::window_specific m_window_specific;
	FG::SwapchainID						m_swapchain_id;
	imgui_renderer_window				m_imgui_window;

	struct shared_data
	{
		FGC::UniquePtr<FGC::VulkanDevice2Initializer> m_device;
		FG::FrameGraph								  m_frame_graph;
		imgui_renderer								  m_imgui_renderer;
		FG::Array<FG::Task>							  m_shared_tasks;
	};

	static inline shared_data m_shared;

	void init(ImGuiContext* imgui_context, ImGuiViewport* viewport, bool primary)
	{
		auto window			 = (GLFWwindow*)viewport->PlatformHandle;
		auto surface_factory = FGC::UniquePtr<FGC::IVulkanSurface>(new platform_device_factory(window));

		if (primary)
		{
			auto new_device			 = std::make_unique<FGC::VulkanDevice2Initializer>();
			auto required_extensions = surface_factory->GetRequiredExtensions();
			new_device->CreateInstance("app_name", "engine_name", new_device->GetRecomendedInstanceLayers(), required_extensions);

			m_window_specific.CreateInstance(surface_factory, new_device->GetVkInstance());

			new_device->ChooseHighPerformanceDevice();
			new_device->CreateLogicalDevice({{VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_SPARSE_BINDING_BIT}, {VK_QUEUE_COMPUTE_BIT}, {VK_QUEUE_TRANSFER_BIT}}, true, true, FGC::Default);

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
			m_shared.m_frame_graph = FG::IFrameGraph::CreateFrameGraph(vulkan_info);

			{
				auto compiler = FG::MakeShared<FG::VPipelineCompiler>(vulkan_info.instance, vulkan_info.physicalDevice, vulkan_info.device);
				compiler->SetCompilationFlags(FG::EShaderCompilationFlags::Quiet);
				m_shared.m_frame_graph->AddPipelineCompiler(compiler);
			}

			m_shared.m_imgui_renderer.init_shared(imgui_context, m_shared.m_frame_graph);
			m_shared.m_device = std::move(new_device);
		}
		else
		{
			m_window_specific.CreateInstance(surface_factory, m_shared.m_device->GetVkInstance());
		}

		m_shared.m_imgui_renderer.init(m_imgui_window, imgui_context, m_shared.m_frame_graph);

		FG::VulkanSwapchainCreateInfo swapchain_info;
		{
			swapchain_info.surface		 = FGC::BitCast<FG::SurfaceVk_t>(m_window_specific.GetVkSurface());
			swapchain_info.surfaceSize.x = uint32_t(viewport->Size.x);
			swapchain_info.surfaceSize.y = uint32_t(viewport->Size.y);
		}
		m_swapchain_id			   = m_shared.m_frame_graph->CreateSwapchain(swapchain_info);
		m_is_primary			   = primary;
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
			swapchain_info.surface		 = FG::BitCast<FG::SurfaceVk_t>(m_window_specific.GetVkSurface());
			swapchain_info.surfaceSize.x = new_width;
			swapchain_info.surfaceSize.y = new_height;

			m_shared.m_frame_graph->WaitIdle();

			m_swapchain_id = m_shared.m_frame_graph->CreateSwapchain(swapchain_info, m_swapchain_id.Release());
		}
	}

	void destroy(ImGuiViewport* viewport)
	{
		m_shared.m_frame_graph->ReleaseResource(m_swapchain_id);
		m_shared.m_imgui_renderer.destroy(m_imgui_window, m_shared.m_frame_graph);

		if (m_is_primary)
		{
			m_shared.m_imgui_renderer.destroy_shared(m_shared.m_frame_graph);
			m_shared.m_frame_graph->Deinitialize();
			m_shared.m_frame_graph = nullptr;

			m_shared.m_device->DestroyLogicalDevice();
			m_shared.m_device->DestroyInstance();
			m_shared.m_device.reset();
		}
	}

	void end_frame()
	{
		CHECK_ERR(m_shared.m_frame_graph->Flush());
	}

	FG::Task load_assets(ImGuiContext* ctx)
	{
		if (m_is_primary && !m_shared.m_imgui_renderer.m_font_texture)
		{
			FG::CommandBuffer cmdbuf = m_shared.m_frame_graph->Begin(FG::CommandBufferDesc{FG::EQueueType::Graphics});
			m_shared.m_shared_tasks.clear();
			auto new_task = m_shared.m_imgui_renderer.create_font_texture(ctx, cmdbuf);
			m_shared.m_frame_graph->Execute(cmdbuf);
			return new_task;
		}

		return nullptr;
	}

	void render_frame(ImGuiContext* ctx, ImGuiViewport* viewport, ImDrawData* draw_data, FG::Task dependent_task)
	{
		if (draw_data->TotalVtxCount > 0)
		{
			FG::CommandBuffer cmdbuf = m_shared.m_frame_graph->Begin(FG::CommandBufferDesc{FG::EQueueType::Graphics});
			CHECK_ERR(cmdbuf);

			{
				auto dep_tasks = FGC::ArrayView<FG::Task>{&dependent_task, dependent_task ? size_t(1) : size_t(0)};

				FG::RawImageID image = cmdbuf->GetSwapchainImage(m_swapchain_id);

				FG::RGBA32f		  _clearColor{0.45f, 0.55f, 0.60f, 1.00f};
				FG::LogicalPassID pass_id = cmdbuf->CreateRenderPass(FG::RenderPassDesc{FG::int2{FG::float2{draw_data->DisplaySize.x, draw_data->DisplaySize.y}}}
																		 .AddViewport(FG::float2{draw_data->DisplaySize.x, draw_data->DisplaySize.y})
																		 .AddTarget(FG::RenderTargetID::Color_0, image, _clearColor, FG::EAttachmentStoreOp::Store));
				FG::Task		  draw_ui = m_shared.m_imgui_renderer.draw(
					 m_imgui_window, draw_data, ctx, cmdbuf, pass_id, dep_tasks, 
					 [&cmdbuf, &pass_id](const ImDrawList& cmd_list, const ImDrawCmd& cmd) -> FG::Task {
						 return imgui_app_fw::mutable_userdata(&cmdbuf, pass_id).call(cmd_list, cmd);
					 });
				FG::Unused(draw_ui);

				CHECK_ERR(m_shared.m_frame_graph->Execute(cmdbuf));
			}
		}
	}
};

struct gui_primary_context
{
	ImGuiContext* m_context = nullptr;

	GLFWwindow* m_window								= nullptr;
	double		m_time									= 0.0;
	bool		m_mouse_pressed[ImGuiMouseButton_COUNT] = {};
	GLFWcursor* m_mouse_cursors[ImGuiMouseCursor_COUNT] = {};
	bool		m_need_monitor_update					= true;
	bool		m_ready									= false;

	gui_primary_context(ImVec2 p, ImVec2 s)
	{
		if (!glfwInit())
		{
			throw std::exception("glfw creation failed");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_window = glfwCreateWindow(int(s.x), int(s.y), "", NULL, NULL);
	}

	~gui_primary_context()
	{
		shutdown_renderer();
		shutdown_window();
		ImGui::DestroyContext();
		m_context = nullptr;

		destroy_window();
	}

	void on_mouse_button(GLFWwindow* window, int button, int action, int mods)
	{
		if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(m_mouse_pressed))
		{
			m_mouse_pressed[button] = true;
		}
	}

	void on_scroll(GLFWwindow* window, double xoffset, double yoffset)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheelH += (float)xoffset;
		io.MouseWheel += (float)yoffset;
	}

	void on_key(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (action == GLFW_PRESS)
		{
			io.KeysDown[key] = true;
		}

		if (action == GLFW_RELEASE)
		{
			io.KeysDown[key] = false;
		}

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

	void on_char(GLFWwindow* window, unsigned int c)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.AddInputCharacter(c);
	}

	void on_window_size(GLFWwindow* window, int, int)
	{
		if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
		{
			if (platform_window_data* data = (platform_window_data*)viewport->PlatformUserData)
			{
				bool ignore_event = (ImGui::GetFrameCount() <= data->m_ignore_window_size_event_frame + 1);
				// data->m_ignore_window_size_event_frame = -1;
				if (ignore_event)
				{
					return;
				}
			}
			viewport->PlatformRequestResize = true;
		}
	}

	void update_monitors()
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
			if (w > 0 && h > 0)
			{
				monitor.WorkPos	 = ImVec2((float)x, (float)y);
				monitor.WorkSize = ImVec2((float)w, (float)h);
			}

			float x_scale, y_scale;
			glfwGetMonitorContentScale(glfw_monitors[n], &x_scale, &y_scale);
			monitor.DpiScale = x_scale;
			platform_io.Monitors.push_back(monitor);
		}
		m_need_monitor_update = false;
	}

	bool init_window(GLFWwindow* window)
	{
		m_window = window;
		m_time	 = 0.0;

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

		io.SetClipboardTextFn = [](void* user_data, const char* text) -> void {
			glfwSetClipboardString((GLFWwindow*)user_data, text);
		};

		io.GetClipboardTextFn = [](void* user_data) -> const char* {
			return glfwGetClipboardString((GLFWwindow*)user_data);
		};

		io.ClipboardUserData = m_window;

		// Create mouse cursors
		// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
		// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
		// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
		GLFWerrorfun prev_error_callback			 = glfwSetErrorCallback(NULL);
		m_mouse_cursors[ImGuiMouseCursor_Arrow]		 = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_TextInput]	 = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_ResizeNS]	 = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_ResizeEW]	 = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_Hand]		 = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_ResizeAll]	 = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
		m_mouse_cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
		glfwSetErrorCallback(prev_error_callback);

		glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) -> void { instance->on_mouse_button(window, button, action, mods); });
		glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) -> void { instance->on_scroll(window, xoffset, yoffset); });
		glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void { instance->on_key(window, key, scancode, action, mods); });
		glfwSetCharCallback(window, [](GLFWwindow* window, unsigned int c) -> void { instance->on_char(window, c); });
		glfwSetMonitorCallback([](GLFWmonitor*, int) -> void { instance->m_need_monitor_update = true; });

		// Update monitors the first time (note: monitor callback are broken in GLFW 3.2 and earlier, see github.com/glfw/glfw/issues/784)
		update_monitors();

		// Our mouse update function expect PlatformHandle to be filled for the main viewport
		ImGuiViewport* main_viewport  = ImGui::GetMainViewport();
		main_viewport->PlatformHandle = (void*)m_window;
#ifdef _WIN32
		main_viewport->PlatformHandleRaw = glfwGetWin32Window(m_window);
#endif
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// Register platform interface (will be coupled with a renderer interface)
			ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

			platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport) -> void {
				platform_window_data* data = IM_NEW(platform_window_data)();
				viewport->PlatformUserData = data;

				glfwWindowHint(GLFW_VISIBLE, false);
				glfwWindowHint(GLFW_FOCUSED, false);
				glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
				glfwWindowHint(GLFW_DECORATED, (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? false : true);
				glfwWindowHint(GLFW_FLOATING, (viewport->Flags & ImGuiViewportFlags_TopMost) ? true : false);
				GLFWwindow* share_window = nullptr;
				data->m_window			 = glfwCreateWindow((int)viewport->Size.x, (int)viewport->Size.y, "No Title Yet", NULL, share_window);
				data->m_window_owned	 = true;
				viewport->PlatformHandle = (void*)data->m_window;
#ifdef _WIN32
				viewport->PlatformHandleRaw = glfwGetWin32Window(data->m_window);
#endif
				glfwSetWindowPos(data->m_window, (int)viewport->Pos.x, (int)viewport->Pos.y);

				// Install GLFW callbacks for secondary viewports
				glfwSetMouseButtonCallback(
					data->m_window, [](GLFWwindow* window, int button, int action, int mods) -> void { instance->on_mouse_button(window, button, action, mods); });

				glfwSetScrollCallback(data->m_window, [](GLFWwindow* window, double xoffset, double yoffset) -> void { instance->on_scroll(window, xoffset, yoffset); });

				glfwSetKeyCallback(
					data->m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void { instance->on_key(window, key, scancode, action, mods); });

				glfwSetCharCallback(data->m_window, [](GLFWwindow* window, unsigned int c) -> void { instance->on_char(window, c); });

				glfwSetWindowSizeCallback(data->m_window, [](GLFWwindow* window, int a, int b) -> void { instance->on_window_size(window, a, b); });

				glfwSetWindowCloseCallback(data->m_window, [](GLFWwindow* window) -> void {
					if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
					{
						viewport->PlatformRequestClose = true;
					}
				});

				glfwSetWindowPosCallback(data->m_window, [](GLFWwindow* window, int, int) -> void {
					if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle(window))
					{
						if (platform_window_data* data = (platform_window_data*)viewport->PlatformUserData)
						{
							bool ignore_event = (ImGui::GetFrameCount() <= data->m_ignore_window_pos_event_frame + 1);
							if (ignore_event)
							{
								return;
							}
						}
						viewport->PlatformRequestMove = true;
					}
				});
			};

			platform_io.Platform_DestroyWindow = [](ImGuiViewport* viewport) -> void {
				if (platform_window_data* data = (platform_window_data*)viewport->PlatformUserData)
				{
					if (data->m_window_owned)
					{
						glfwDestroyWindow(data->m_window);
					}
					data->m_window = NULL;
					IM_DELETE(data);
				}
				viewport->PlatformUserData = viewport->PlatformHandle = NULL;
			};

			platform_io.Platform_ShowWindow = [](ImGuiViewport* viewport) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;

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

				glfwShowWindow(data->m_window);
			};

			platform_io.Platform_SetWindowPos = [](ImGuiViewport* viewport, ImVec2 pos) -> void {
				platform_window_data* data			  = (platform_window_data*)viewport->PlatformUserData;
				data->m_ignore_window_pos_event_frame = ImGui::GetFrameCount();
				glfwSetWindowPos(data->m_window, (int)pos.x, (int)pos.y);
			};

			platform_io.Platform_GetWindowPos = [](ImGuiViewport* viewport) -> ImVec2 {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				int					  x = 0, y = 0;
				glfwGetWindowPos(data->m_window, &x, &y);
				return ImVec2((float)x, (float)y);
			};

			platform_io.Platform_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) -> void {
				platform_window_data* data			   = (platform_window_data*)viewport->PlatformUserData;
				data->m_ignore_window_size_event_frame = ImGui::GetFrameCount();
				glfwSetWindowSize(data->m_window, (int)size.x, (int)size.y);
			};

			platform_io.Platform_GetWindowSize = [](ImGuiViewport* viewport) -> ImVec2 {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				int					  w = 0, h = 0;
				glfwGetWindowSize(data->m_window, &w, &h);
				return ImVec2((float)w, (float)h);
			};

			platform_io.Platform_SetWindowFocus = [](ImGuiViewport* viewport) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				glfwFocusWindow(data->m_window);
			};

			platform_io.Platform_GetWindowFocus = [](ImGuiViewport* viewport) -> bool {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				return glfwGetWindowAttrib(data->m_window, GLFW_FOCUSED) != 0;
			};

			platform_io.Platform_GetWindowMinimized = [](ImGuiViewport* viewport) -> bool {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				return glfwGetWindowAttrib(data->m_window, GLFW_ICONIFIED) != 0;
			};

			platform_io.Platform_SetWindowTitle = [](ImGuiViewport* viewport, const char* title) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				glfwSetWindowTitle(data->m_window, title);
			};

			platform_io.Platform_RenderWindow = [](ImGuiViewport* viewport, void*) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
			};

			platform_io.Platform_SwapBuffers = [](ImGuiViewport* viewport, void*) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
			};

			platform_io.Platform_SetWindowAlpha = [](ImGuiViewport* viewport, float alpha) -> void {
				platform_window_data* data = (platform_window_data*)viewport->PlatformUserData;
				glfwSetWindowOpacity(data->m_window, alpha);
			};

			//#if GLFW_HAS_VULKAN
			//    platform_io.Platform_CreateVkSurface = ImGui_ImplGlfw_CreateVkSurface;
			//#endif

#if HAS_WIN32_IME
			platform_io.Platform_SetImeInputPos = [](ImGuiViewport* viewport, ImVec2 pos) -> void {
				COMPOSITIONFORM cf = {CFS_FORCE_POSITION, {(LONG)(pos.x - viewport->Pos.x), (LONG)(pos.y - viewport->Pos.y)}, {0, 0, 0, 0}};
				if (HWND hwnd = (HWND)viewport->PlatformHandleRaw)
					if (HIMC himc = ::ImmGetContext(hwnd))
					{
						::ImmSetCompositionWindow(himc, &cf);
						::ImmReleaseContext(hwnd, himc);
					}
			};
#endif

			ImGuiViewport*		  main_viewport = ImGui::GetMainViewport();
			platform_window_data* data			= IM_NEW(platform_window_data)();
			data->m_window						= m_window;
			data->m_window_owned				= false;
			main_viewport->PlatformUserData		= data;
			main_viewport->PlatformHandle		= (void*)m_window;
		}

		glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int a, int b) -> void { instance->on_window_size(window, a, b); });

		return true;
	}

	void shutdown_window()
	{
		glfwSetMouseButtonCallback(m_window, nullptr);
		glfwSetScrollCallback(m_window, nullptr);
		glfwSetKeyCallback(m_window, nullptr);
		glfwSetCharCallback(m_window, nullptr);
		glfwSetMonitorCallback(nullptr);

		for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
		{
			glfwDestroyCursor(m_mouse_cursors[cursor_n]);
			m_mouse_cursors[cursor_n] = NULL;
		}
	}

	void update_mouse_pos_and_buttons()
	{
		// Update buttons
		ImGuiIO& io = ImGui::GetIO();
		for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
		{
			// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
			io.MouseDown[i]	   = m_mouse_pressed[i] || glfwGetMouseButton(m_window, i) != 0;
			m_mouse_pressed[i] = false;
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
				{
					io.MouseDown[i] |= glfwGetMouseButton(window, i) != 0;
				}
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
			{
				io.MouseHoveredViewport = viewport->ID;
			}
		}
	}

	void update_mouse_cursor()
	{
		ImGuiIO& io = ImGui::GetIO();
		if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		{
			return;
		}

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
				glfwSetCursor(window, m_mouse_cursors[imgui_cursor] ? m_mouse_cursors[imgui_cursor] : m_mouse_cursors[ImGuiMouseCursor_Arrow]);
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
		}
	}

	void update_gamepads()
	{
		ImGuiIO& io = ImGui::GetIO();
		memset(io.NavInputs, 0, sizeof(io.NavInputs));
		if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
		{
			return;
		}

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
		{
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
		}
		else
		{
			io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
		}
	}

	bool init_renderer(ImGuiContext* imgui_context)
	{
		// Setup back-end capabilities flags
		ImGuiIO& io			   = ImGui::GetIO();
		io.BackendRendererName = "imgui_impl_glfw_vulkan";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional) // FIXME-VIEWPORT: Actually unfinished..

		// Create a dummy platform_renderer_data holder for the main viewport,
		// Since this is created and managed by the application, we will only use the ->Resources[] fields.
		ImGuiViewport* main_viewport = ImGui::GetMainViewport();

		auto primary_viewport_data = IM_NEW(platform_renderer_data);
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
			};
		}

		return true;
	}

	void shutdown_renderer()
	{
		// Manually delete main viewport render resources in-case we haven't initialized for viewports
		ImGuiViewport*			main_viewport	   = ImGui::GetMainViewport();
		platform_renderer_data* main_viewport_data = (platform_renderer_data*)main_viewport->RendererUserData;
		main_viewport->RendererUserData			   = nullptr;

		// Clean up windows and device objects
		ImGui::DestroyPlatformWindows();

		// destroy_device_objects();

		main_viewport_data->destroy(main_viewport);
		IM_DELETE(main_viewport_data);
	}

	void new_frame()
	{
		ImGuiIO& io = ImGui::GetIO();
		IM_ASSERT(
			io.Fonts->IsBuilt() &&
			"Font atlas not built! It is generally built by the renderer backend. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

		int w, h;
		int display_w, display_h;
		glfwGetWindowSize(m_window, &w, &h);
		glfwGetFramebufferSize(m_window, &display_w, &display_h);
		io.DisplaySize = ImVec2((float)w, (float)h);
		if (w > 0 && h > 0)
		{
			io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);
		}

		if (m_need_monitor_update)
		{
			update_monitors();
		}

		double current_time = glfwGetTime();
		io.DeltaTime		= m_time > 0.0 ? (float)(current_time - m_time) : (float)(1.0f / 60.0f);
		m_time				= current_time;

		update_mouse_pos_and_buttons();
		update_mouse_cursor();
		update_gamepads();
	}

	void create_secondary_window(ImGuiViewport* viewport)
	{
		platform_renderer_data* data = IM_NEW(platform_renderer_data)();
		viewport->RendererUserData	 = data;
		data->init(ImGui::GetCurrentContext(), viewport, false);
	}

	void destroy_secondary_window(ImGuiViewport* viewport)
	{
		// The main viewport (owned by the application) will always have RendererUserData == nullptr since we didn't create the data for it.
		if (platform_renderer_data* data = (platform_renderer_data*)viewport->RendererUserData)
		{
			data->destroy(viewport);
			IM_DELETE(data);
		}
		viewport->RendererUserData = nullptr;
	}

	void set_secondary_window_size(ImGuiViewport* viewport, ImVec2 size)
	{
		platform_renderer_data* data = (platform_renderer_data*)viewport->RendererUserData;
		data->handle_resize(viewport);
	}

	FG::Task m_pending_task = nullptr;

	void render_secondary_window(ImGuiViewport* viewport)
	{
		const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

		platform_renderer_data* data = (platform_renderer_data*)viewport->RendererUserData;
		data->render_frame(ImGui::GetCurrentContext(), viewport, viewport->DrawData, m_pending_task);
	}

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

		if (ImGuiViewport* main_viewport = ImGui::GetMainViewport(); main_viewport->PlatformRequestResize)
		{
			((platform_renderer_data*)main_viewport->RendererUserData)->handle_resize(main_viewport);
			main_viewport->PlatformRequestResize = false;
		}

		ImGui::NewFrame();
	}

	void end_frame(ImVec4 clear_color)
	{
		ImGui::Render();

		ImGuiViewport*			main_viewport	   = ImGui::GetMainViewport();
		platform_renderer_data* main_viewport_data = (platform_renderer_data*)main_viewport->RendererUserData;

		if (main_viewport->PlatformRequestResize)
		{
			main_viewport_data->handle_resize(main_viewport);
			main_viewport->PlatformRequestResize = false;
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
		}

		m_pending_task = main_viewport_data->load_assets(m_context);
		main_viewport_data->render_frame(m_context, main_viewport, ImGui::GetDrawData(), m_pending_task);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
		}
		main_viewport_data->end_frame();
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
		init_window(m_window);

		init_renderer(m_context);

		// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
		// - If the file cannot be loaded, the function will return nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and
		// quit).
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

		basist::basisu_transcoder_init();

		return true;
	}

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

FG::IFrameGraph* imgui_app_fw::get_framegraph_instance()
{
	assert(ImGui::GetMainViewport() && ImGui::GetMainViewport()->RendererUserData);
	return platform_renderer_data::m_shared.m_frame_graph.get();
}
