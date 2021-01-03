#include <imgui_app_fw.h>
#include <imgui_app_fw_rendering.h>

//struct custom_scene
//{
//	void init(FG::IFrameGraph* fg)
//	{
//		FG::GraphicsPipelineDesc pipeline_desc;
//
//		pipeline_desc.AddShader(FG::EShader::Vertex, FG::EShaderLangFormat::VKSL_100, "main", R"#(
//			#pragma shader_stage(vertex)
//			#extension GL_ARB_separate_shader_objects : enable
//			#extension GL_ARB_shading_language_420pack : enable
//
//			layout(location=0) out vec3  v_Color;
//
//			const vec2	g_Positions[3] = vec2[](
//				vec2(0.0, -0.5),
//				vec2(0.5, 0.5),
//				vec2(-0.5, 0.5)
//			);
//
//			const vec3	g_Colors[3] = vec3[](
//				vec3(1.0, 0.0, 0.0),
//				vec3(0.0, 1.0, 0.0),
//				vec3(0.0, 0.0, 1.0)
//			);
//
//			void main() {
//				gl_Position	= vec4( g_Positions[gl_VertexIndex], 0.0, 1.0 );
//				v_Color		= g_Colors[gl_VertexIndex];
//			}
//			)#");
//
//		pipeline_desc.AddShader(FG::EShader::Fragment, FG::EShaderLangFormat::VKSL_100, "main", R"#(
//			#pragma shader_stage(fragment)
//			#extension GL_ARB_separate_shader_objects : enable
//			#extension GL_ARB_shading_language_420pack : enable
//
//			layout(location=0) out vec4  out_Color;
//
//			layout(location=0) in  vec3  v_Color;
//
//			void main() {
//				out_Color = vec4(v_Color, 1.0);
//			}
//			)#");
//	
//		const FG::uint2		view_size	= {800, 600};
//		
//		FG::ImageID			image		= fg->CreateImage( FG::ImageDesc{}.SetDimension( view_size ).SetFormat( FG::EPixelFormat::RGBA8_UNorm )
//																		.SetUsage( FG::EImageUsage::ColorAttachment | FG::EImageUsage::TransferSrc ),
//															    FG::Default, "RenderTarget" );
//
//		FG::GPipelineID		pipeline	= fg->CreatePipeline( pipeline_desc );
//	}
//};

// Main code
int main(int, char**)
{
	if (imgui_app_fw::select_platform(imgui_app_fw::platform::glfw_vulkan))
	{
		if (imgui_app_fw::init())
		{
			imgui_app_fw::set_window_title("Hello!");

			// Our state
			bool   show_demo_window	   = true;
			bool   show_another_window = false;
			ImVec4 clear_color		   = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

			while (imgui_app_fw::pump())
			{
				imgui_app_fw::begin_frame();

				// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
				if (show_demo_window)
				{
					ImGui::ShowDemoWindow(&show_demo_window);
				}

				// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
				{
					static float f		 = 0.0f;
					static int	 counter = 0;

					ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

					ImGui::Text("This is some useful text.");		   // Display some text (you can use a format strings too)
					ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
					ImGui::Checkbox("Another Window", &show_another_window);

					ImGui::SliderFloat("float", &f, 0.0f, 1.0f);			// Edit 1 float using a slider from 0.0f to 1.0f
					ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

					if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
					{
						counter++;
					}

					ImGui::SameLine();
					ImGui::Text("counter = %d", counter);

					ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
					ImGui::End();
				}

				// 3. Show another simple window.
				if (show_another_window)
				{
					ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_FirstUseEver);
					ImGui::Begin(
						"Another Window",
						&show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)

					ImGui::GetWindowDrawList()->AddCallback(
						[](const ImDrawList* parent_list, const ImDrawCmd* cmd) -> void {
							auto user_data = imgui_app_fw::mutable_userdata::get_from_draw_cmd(cmd);
							auto pos_x	   = cmd->ClipRect.x;
							auto width	   = (cmd->ClipRect.z - cmd->ClipRect.x);
							auto pos_y	   = cmd->ClipRect.y;
							auto height	   = (cmd->ClipRect.w - cmd->ClipRect.y);
						},
						nullptr);

					ImGui::Text("Hello from another window!");
					if (ImGui::Button("Close Me"))
					{
						show_another_window = false;
					}

					ImGui::End();
				}

				imgui_app_fw::end_frame(clear_color);
			}

			imgui_app_fw::destroy();
		}
	}

	return 0;
}
