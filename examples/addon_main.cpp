#include <imgui_app_fw.h>
#include <implot.h>
#include <TextEditor.h>
#include <ImGuiFileDialog.h>
#include <imgui_markdown.h>
#include <imgui_console.h>
#include <imgui_node_editor.h>

static bool canValidateDialog = false;

inline void InfosPane(std::string vFilter, igfd::UserDatas vUserDatas, bool* vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
	ImGui::TextColored(ImVec4(0, 1, 1, 1), "Infos Pane");

	ImGui::Text("Selected Filter : %s", vFilter.c_str());

	const char* userDatas = (const char*)vUserDatas;
	if (userDatas)
		ImGui::Text("User Datas : %s", userDatas);

	ImGui::Checkbox("if not checked you cant validate the dialog", &canValidateDialog);

	if (vCantContinue)
		*vCantContinue = canValidateDialog;
}

void LinkCallback(ImGui::MarkdownLinkCallbackData data_);
inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data_);

// Main code
int main(int, char**)
{
	if (imgui_app_fw::select_platform(imgui_app_fw::platform::glfw_vulkan))
	{
		if (imgui_app_fw::init())
		{
			imgui_app_fw::set_window_title("Hello!");
			
			ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
			bool show_demo_window = true;

			ImGuiConsole console;

			// Log example information:
			console.System().Log(csys::ItemType::INFO) << "Welcome to the imgui-console example!" << csys::endl;
			console.System().Log(csys::ItemType::INFO) << "The following variables have been exposed to the console:" << csys::endl << csys::endl;
			console.System().Log(csys::ItemType::INFO) << "\tbackground_color - set: [int int int int]" << csys::endl;
			console.System().Log(csys::ItemType::INFO) << csys::endl << "Try running the following command:" << csys::endl;
			console.System().Log(csys::ItemType::INFO) << "\tset background_color [255 0 0 255]" << csys::endl << csys::endl;


			namespace ed = ax::NodeEditor;
			static ed::EditorContext* g_Context = nullptr;
			ed::Config config;
			config.SettingsFile = "Simple.json";
			g_Context = ed::CreateEditor(&config);

			TextEditor editor;
			auto lang = TextEditor::LanguageDefinition::CPlusPlus();

			// set your own known preprocessor symbols...
			static const char* ppnames[] = { "NULL", "PM_REMOVE",
				"ZeroMemory", "DXGI_SWAP_EFFECT_DISCARD", "D3D_FEATURE_LEVEL", "D3D_DRIVER_TYPE_HARDWARE", "WINAPI","D3D11_SDK_VERSION", "assert" };
			// ... and their corresponding values
			static const char* ppvalues[] = {
				"#define NULL ((void*)0)",
				"#define PM_REMOVE (0x0001)",
				"Microsoft's own memory zapper function\n(which is a macro actually)\nvoid ZeroMemory(\n\t[in] PVOID  Destination,\n\t[in] SIZE_T Length\n); ",
				"enum DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_DISCARD = 0",
				"enum D3D_FEATURE_LEVEL",
				"enum D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE  = ( D3D_DRIVER_TYPE_UNKNOWN + 1 )",
				"#define WINAPI __stdcall",
				"#define D3D11_SDK_VERSION (7)",
				" #define assert(expression) (void)(                                                  \n"
				"    (!!(expression)) ||                                                              \n"
				"    (_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \n"
				" )"
			};

			for (int i = 0; i < sizeof(ppnames) / sizeof(ppnames[0]); ++i)
			{
				TextEditor::Identifier id;
				id.mDeclaration = ppvalues[i];
				lang.mPreprocIdentifiers.insert(std::make_pair(std::string(ppnames[i]), id));
			}

			// set your own identifiers
			static const char* identifiers[] = {
				"HWND", "HRESULT", "LPRESULT","D3D11_RENDER_TARGET_VIEW_DESC", "DXGI_SWAP_CHAIN_DESC","MSG","LRESULT","WPARAM", "LPARAM","UINT","LPVOID",
				"ID3D11Device", "ID3D11DeviceContext", "ID3D11Buffer", "ID3D11Buffer", "ID3D10Blob", "ID3D11VertexShader", "ID3D11InputLayout", "ID3D11Buffer",
				"ID3D10Blob", "ID3D11PixelShader", "ID3D11SamplerState", "ID3D11ShaderResourceView", "ID3D11RasterizerState", "ID3D11BlendState", "ID3D11DepthStencilState",
				"IDXGISwapChain", "ID3D11RenderTargetView", "ID3D11Texture2D", "TextEditor" };
			static const char* idecls[] =
			{
				"typedef HWND_* HWND", "typedef long HRESULT", "typedef long* LPRESULT", "struct D3D11_RENDER_TARGET_VIEW_DESC", "struct DXGI_SWAP_CHAIN_DESC",
				"typedef tagMSG MSG\n * Message structure","typedef LONG_PTR LRESULT","WPARAM", "LPARAM","UINT","LPVOID",
				"ID3D11Device", "ID3D11DeviceContext", "ID3D11Buffer", "ID3D11Buffer", "ID3D10Blob", "ID3D11VertexShader", "ID3D11InputLayout", "ID3D11Buffer",
				"ID3D10Blob", "ID3D11PixelShader", "ID3D11SamplerState", "ID3D11ShaderResourceView", "ID3D11RasterizerState", "ID3D11BlendState", "ID3D11DepthStencilState",
				"IDXGISwapChain", "ID3D11RenderTargetView", "ID3D11Texture2D", "class TextEditor" };
			for (int i = 0; i < sizeof(identifiers) / sizeof(identifiers[0]); ++i)
			{
				TextEditor::Identifier id;
				id.mDeclaration = std::string(idecls[i]);
				lang.mIdentifiers.insert(std::make_pair(std::string(identifiers[i]), id));
			}
			editor.SetLanguageDefinition(lang);
			//editor.SetPalette(TextEditor::GetLightPalette());

			// error markers
			TextEditor::ErrorMarkers markers;
			markers.insert(std::make_pair<int, std::string>(6, "Example error here:\nInclude file not found: \"TextEditor.h\""));
			markers.insert(std::make_pair<int, std::string>(41, "Another example error"));
			editor.SetErrorMarkers(markers);

			// "breakpoint" markers
			//TextEditor::Breakpoints bpts;
			//bpts.insert(24);
			//bpts.insert(47);
			//editor.SetBreakpoints(bpts);

			static const char* fileToEdit = "ImGuiColorTextEdit/TextEditor.cpp";

#if 1
			bool show_plot_demo_window = true;
			auto plotContext = ImPlot::CreateContext();
#endif

#if 0
			// Our state
			bool show_another_window = false;

			//	static const char* fileToEdit = "test.cpp";

			igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".cpp", ImVec4(1.0f, 1.0f, 0.0f, 0.9f));
			igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".h", ImVec4(0.0f, 1.0f, 0.0f, 0.9f));
			igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".hpp", ImVec4(0.0f, 0.0f, 1.0f, 0.9f));
			igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".md", ImVec4(1.0f, 0.0f, 1.0f, 0.9f));
			//igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".png", ImVec4(0.0f, 1.0f, 1.0f, 0.9f), ICON_IGFD_FILE_PIC); // add an icon for the filter type
			igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".gif", ImVec4(0.0f, 1.0f, 0.5f, 0.9f), "[GIF]"); // add an text for a filter type

#ifdef USE_BOOKMARK
			// load bookmarks
			std::ifstream docFile("bookmarks.conf", std::ios::in);
			if (docFile.is_open())
			{
				std::stringstream strStream;
				strStream << docFile.rdbuf();//read the file
				igfd::ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
				docFile.close();
		}
#endif

#endif

			ImGui::MarkdownConfig mdConfig;
			mdConfig.linkCallback = LinkCallback;
			mdConfig.tooltipCallback = NULL;
			mdConfig.imageCallback = ImageCallback;
			//mdConfig.linkIcon =             ICON_FA_LINK;
			//mdConfig.headingFormats[0] =    { H1, true };
			//mdConfig.headingFormats[1] =    { H2, true };
			//mdConfig.headingFormats[2] =    { H3, false };
			mdConfig.userData = NULL;

			const std::string markdownText = (const char*)u8R"(# H1 Header: Text and Links
You can add [links like this one to enkisoftware](https://www.enkisoftware.com/) and lines will wrap well.
## H2 Header: indented text.
  This text has an indent (two leading spaces).
    This one has two.
### H3 Header: Lists
  * Unordered lists
    * Lists can be indented with two extra spaces.
  * Lists can have [links like this one to Avoyd](https://www.avoyd.com/)
)";
			
			while (imgui_app_fw::pump())
			{
				imgui_app_fw::begin_frame();

				if (show_demo_window)
					ImGui::ShowDemoWindow(&show_demo_window);

				console.Draw();

				ed::SetCurrentEditor(g_Context);
				ed::Begin("My Editor", ImVec2(0.0, 0.0f));
				int uniqueId = 1;
				// Start drawing nodes.
				ed::BeginNode(uniqueId++);
					ImGui::Text("Node A");
					ed::BeginPin(uniqueId++, ed::PinKind::Input);
						ImGui::Text("-> In");
					ed::EndPin();
					ImGui::SameLine();
					ed::BeginPin(uniqueId++, ed::PinKind::Output);
						ImGui::Text("Out ->");
					ed::EndPin();
				ed::EndNode();
				ed::End();
				ed::SetCurrentEditor(nullptr);

#if 0
				// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).

				// 3. Show another simple window.
				if (show_another_window)
				{
					ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
					ImGui::Text("Hello from another window!");

					if (ImGui::Button("Close Me"))
						show_another_window = false;

					imnodes::BeginNodeEditor();
					imnodes::BeginNode(1);

					imnodes::BeginNodeTitleBar();
					ImGui::TextUnformatted("simple node :)");
					imnodes::EndNodeTitleBar();

					imnodes::BeginInputAttribute(2);
					ImGui::Text("input");
					imnodes::EndInputAttribute();

					imnodes::BeginOutputAttribute(3);
					ImGui::Indent(40);
					ImGui::Text("output");
					imnodes::EndOutputAttribute();

					imnodes::EndNode();
					imnodes::EndNodeEditor();

					ImGui::End();
				}

				{
					ImGui::Text("imGuiFileDialog Demo %s : ", IMGUIFILEDIALOG_VERSION);
					ImGui::Indent();
					{
#ifdef USE_EXPLORATION_BY_KEYS
						static float flashingAttenuationInSeconds = 1.0f;
						if (ImGui::Button("R##resetflashlifetime"))
						{
							flashingAttenuationInSeconds = 1.0f;
							igfd::ImGuiFileDialog::Instance()->SetFlashingAttenuationInSeconds(flashingAttenuationInSeconds);
						}
						ImGui::SameLine();
						ImGui::PushItemWidth(200);
						if (ImGui::SliderFloat("Flash lifetime (s)", &flashingAttenuationInSeconds, 0.01f, 5.0f))
							igfd::ImGuiFileDialog::Instance()->SetFlashingAttenuationInSeconds(flashingAttenuationInSeconds);
						ImGui::PopItemWidth();
#endif
						ImGui::Separator();
						ImGui::Text("Constraints is used here for define min/ax fiel dialog size");
						ImGui::Separator();
						static bool standardDialogMode = true;
						ImGui::Text("Open Mode : ");
						ImGui::SameLine();

						standardDialogMode = true;

						if (ImGui::Button("Open File Dialog"))
						{
							const char* filters = ".*,.cpp,.h,.hpp";
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey",
									"Choose a File", filters, ".");
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey",
									"Choose a File", filters, ".");
						}
						if (ImGui::Button("Open File Dialog with collections of filters"))
						{
							const char* filters = "Source files (*.cpp *.h *.hpp){.cpp,.h,.hpp},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md";
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey",
									"Choose a File", filters, ".");
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey",
									"Choose a File", filters, ".");
						}
						if (ImGui::Button("Open File Dialog with selection of 5 items"))
						{
							const char* filters = ".*,.cpp,.h,.hpp";
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey",
									"Choose a File", filters, ".", 5);
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey",
									"Choose a File", filters, ".", 5);
						}
						if (ImGui::Button("Open File Dialog with infinite selection"))
						{
							const char* filters = ".*,.cpp,.h,.hpp";
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey",
									"Choose a File", filters, ".", 0);
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey",
									"Choose a File", filters, ".", 0);
						}
						if (ImGui::Button("Save File Dialog with a custom pane"))
						{
							const char* filters = "C++ File (*.cpp){.cpp}";
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey",
									"Choose a File", filters,
									".", "", std::bind(&InfosPane, std::placeholders::_1, std::placeholders::_2,
										std::placeholders::_3), 350, 1, igfd::UserDatas("SaveFile"));
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey",
									"Choose a File", filters,
									".", "", std::bind(&InfosPane, std::placeholders::_1, std::placeholders::_2,
										std::placeholders::_3), 350, 1, igfd::UserDatas("SaveFile"));
						}
						if (ImGui::Button("Open Directory Dialog"))
						{
							// set filters to 0 for open directory chooser
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseDirDlgKey",
									"Choose a Directory", 0, ".");
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseDirDlgKey",
									"Choose a Directory", 0, ".");
						}
						if (ImGui::Button("Open Directory Dialog with selection of 5 items"))
						{
							// set filters to 0 for open directory chooser
							if (standardDialogMode)
								igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseDirDlgKey",
									"Choose a Directory", 0, ".", 5);
							else
								igfd::ImGuiFileDialog::Instance()->OpenModal("ChooseDirDlgKey",
									"Choose a Directory", 0, ".", 5);
						}

						// you can define your flags and min/max window size (theses three settings ae defined by default :
						// flags => ImGuiWindowFlags_NoCollapse
						// minSize => 0,0
						// maxSize => FLT_MAX, FLT_MAX (defined is float.h)

						if (igfd::ImGuiFileDialog::Instance()->FileDialog("ChooseFileDlgKey",
							ImGuiWindowFlags_NoCollapse))
						{
							if (igfd::ImGuiFileDialog::Instance()->IsOk)
							{
								std::string filePathName = igfd::ImGuiFileDialog::Instance()->GetFilePathName();
								std::string filePath = igfd::ImGuiFileDialog::Instance()->GetCurrentPath();
								std::string filter = igfd::ImGuiFileDialog::Instance()->GetCurrentFilter();
								// here convert from string because a string was passed as a userDatas, but it can be what you want
								std::string userDatas;
								if (igfd::ImGuiFileDialog::Instance()->GetUserDatas())
									userDatas = std::string((const char*)igfd::ImGuiFileDialog::Instance()->GetUserDatas());
								auto selection = igfd::ImGuiFileDialog::Instance()->GetSelection(); // multiselection

								// action
							}
							igfd::ImGuiFileDialog::Instance()->CloseDialog("ChooseFileDlgKey");
						}

						if (igfd::ImGuiFileDialog::Instance()->FileDialog("ChooseDirDlgKey",
							ImGuiWindowFlags_NoCollapse))
						{
							if (igfd::ImGuiFileDialog::Instance()->IsOk)
							{
								std::string filePathName = igfd::ImGuiFileDialog::Instance()->GetFilePathName();
								std::string filePath = igfd::ImGuiFileDialog::Instance()->GetCurrentPath();
								std::string filter = igfd::ImGuiFileDialog::Instance()->GetCurrentFilter();
								// here convert from string because a string was passed as a userDatas, but it can be what you want
								std::string userDatas;
								if (igfd::ImGuiFileDialog::Instance()->GetUserDatas())
									userDatas = std::string((const char*)igfd::ImGuiFileDialog::Instance()->GetUserDatas());
								auto selection = igfd::ImGuiFileDialog::Instance()->GetSelection(); // multiselection

								// action
							}
							igfd::ImGuiFileDialog::Instance()->CloseDialog("ChooseDirDlgKey");
						}
					}
					ImGui::Unindent();
				}

#endif
				{
					auto cpos = editor.GetCursorPosition();
					ImGui::Begin("Text Editor Demo", nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
					ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
					if (ImGui::BeginMenuBar())
					{
						if (ImGui::BeginMenu("File"))
						{
							if (ImGui::MenuItem("Save"))
							{
								auto textToSave = editor.GetText();
								/// save text....
							}
							if (ImGui::MenuItem("Quit", "Alt-F4"))
								break;
							ImGui::EndMenu();
						}
						if (ImGui::BeginMenu("Edit"))
						{
							bool ro = editor.IsReadOnly();
							if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
								editor.SetReadOnly(ro);
							ImGui::Separator();

							if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !ro && editor.CanUndo()))
								editor.Undo();
							if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
								editor.Redo();

							ImGui::Separator();

							if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection()))
								editor.Copy();
							if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
								editor.Cut();
							if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
								editor.Delete();
							if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
								editor.Paste();

							ImGui::Separator();

							if (ImGui::MenuItem("Select all", nullptr, nullptr))
								editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("View"))
						{
							if (ImGui::MenuItem("Dark palette"))
								editor.SetPalette(TextEditor::GetDarkPalette());
							if (ImGui::MenuItem("Light palette"))
								editor.SetPalette(TextEditor::GetLightPalette());
							if (ImGui::MenuItem("Retro blue palette"))
								editor.SetPalette(TextEditor::GetRetroBluePalette());
							ImGui::EndMenu();
						}
						ImGui::EndMenuBar();
					}

					ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
						editor.IsOverwrite() ? "Ovr" : "Ins",
						editor.CanUndo() ? "*" : " ",
						editor.GetLanguageDefinition().mName.c_str(), fileToEdit);

					editor.Render("TextEditor");
					ImGui::End();
				}

				{
					static float f = 0.0f;
					static int counter = 0;

					ImGui::Begin("markdown editor");

					ImGui::Markdown(markdownText.c_str(), markdownText.length(), mdConfig);

					ImGui::End();
				}

#if 1
				{
					ImPlot::SetCurrentContext(plotContext);
					ImPlot::ShowDemoWindow(&show_plot_demo_window);
				}
#endif

				imgui_app_fw::end_frame(clear_color);
			}

			ed::DestroyEditor(g_Context);
			g_Context = nullptr;
#if 1
			ImPlot::DestroyContext(plotContext);
#endif
			imgui_app_fw::destroy();
		}
	}

	return 0;
}

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h>
#include <string>

void LinkCallback(ImGui::MarkdownLinkCallbackData data_)
{
	std::string url(data_.link, data_.linkLength);
	if (!data_.isImage)
	{
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}
}

inline ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data_)
{
	// In your application you would load an image based on data_ input. Here we just use the imgui font texture.
	ImTextureID image = ImGui::GetIO().Fonts->TexID;
	// > C++14 can use ImGui::MarkdownImageData imageData{ true, false, image, ImVec2( 40.0f, 20.0f ) };
	ImGui::MarkdownImageData imageData;
	imageData.isValid = true;
	imageData.useLinkCallback = false;
	imageData.user_texture_id = image;
	imageData.size = ImVec2(40.0f, 20.0f);
	return imageData;
}