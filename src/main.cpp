#include "../include/main.hpp"

#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "../external/imgui/backends/imgui_impl_opengl3.h"
#include "../external/imgui/imgui.h"
#include "../external/imguifiledialog/ImGuiFileDialog.h"
#include "../include/glad/glad.h"
#include "../include/imfilebrowser.h"
#include "spdlog/spdlog.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace fs = std::filesystem;

struct AppState
{
	char path_buffer[512] = "";
	bool processing		  = false;
	bool show_success	  = false;
	bool show_error		  = false;
	std::string error_message;
	std::string selected_path;
	int processed_files = 0;

	int selected_theme		= 0;
	ImVec4 custom_color		= ImVec4(0.45F, 0.55F, 0.60F, 1.00F);
	ImVec4 temp_color		= ImVec4(0.45F, 0.55F, 0.60F, 1.00F);
	bool show_color_picker	= false;
	ImVec2 color_picker_pos = ImVec2(0, 0);

	ImVec4 ui_button_color		= ImVec4(0.26F, 0.59F, 0.98F, 1.00F);
	ImVec4 ui_input_color		= ImVec4(0.16F, 0.29F, 0.48F, 1.00F);
	ImVec4 temp_ui_button_color = ImVec4(0.26F, 0.59F, 0.98F, 1.00F);
	ImVec4 temp_ui_input_color	= ImVec4(0.16F, 0.29F, 0.48F, 1.00F);
	bool show_ui_color_picker	= false;
	ImVec2 ui_color_picker_pos	= ImVec2(0, 0);

	bool is_saving		= false;
	bool save_completed = false;
	std::chrono::steady_clock::time_point save_completed_time;

	ImGui::FileBrowser file_browser{ImGuiFileBrowserFlags_SelectDirectory};
	std::vector<std::pair<std::string, bool>> log_lines;
};

ImVec4 reduce_opacity(const ImVec4& color, float reduction = 0.2F)
{
	float new_alpha = color.w * (1.0F - reduction);
	new_alpha		= std::clamp(new_alpha, 0.0F, 1.0F);
	return ImVec4(color.x, color.y, color.z, color.w);
}

void apply_ui_colors(const AppState& state)
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec4 button_color = reduce_opacity(state.ui_button_color);
	ImVec4 input_color	= reduce_opacity(state.ui_input_color);

	style.Colors[ImGuiCol_Button]		 = button_color;
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(button_color.x * 1.2F, button_color.y * 1.2F, button_color.z * 1.2F, button_color.w);
	style.Colors[ImGuiCol_ButtonActive]	 = ImVec4(button_color.x * 0.8F, button_color.y * 0.8F, button_color.z * 0.8F, button_color.w);

	style.Colors[ImGuiCol_FrameBg]		  = input_color;
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(input_color.x * 1.1F, input_color.y * 1.1F, input_color.z * 1.1F, input_color.w);
	style.Colors[ImGuiCol_FrameBgActive]  = ImVec4(input_color.x * 1.3F, input_color.y * 1.3F, input_color.z * 1.3F, input_color.w);
}

void save_settings(AppState& state)
{
	state.is_saving		 = true;
	state.save_completed = false;

	nlohmann::json j;
	j["selected_theme"]	 = state.selected_theme;
	j["custom_color"]	 = {state.custom_color.x, state.custom_color.y, state.custom_color.z, state.custom_color.w};
	j["ui_button_color"] = {state.ui_button_color.x, state.ui_button_color.y, state.ui_button_color.z, state.ui_button_color.w};
	j["ui_input_color"]	 = {state.ui_input_color.x, state.ui_input_color.y, state.ui_input_color.z, state.ui_input_color.w};

	std::ofstream file("settings.json");
	if (file)
	{
		file << j.dump(4);
	}

	state.is_saving			  = false;
	state.save_completed	  = true;
	state.save_completed_time = std::chrono::steady_clock::now();
}

void load_settings(AppState& state)
{
	std::ifstream file("settings.json");
	if (!file)
	{
		return;
	}

	nlohmann::json j;
	file >> j;

	if (j.contains("selected_theme"))
	{
		state.selected_theme = j["selected_theme"].get<int>();
	}

	if (j.contains("custom_color") && j["custom_color"].is_array())
	{
		auto arr		   = j["custom_color"];
		state.custom_color = ImVec4(arr[0], arr[1], arr[2], arr[3]);
		state.temp_color   = state.custom_color;
	}

	if (j.contains("ui_button_color") && j["ui_button_color"].is_array())
	{
		auto arr				   = j["ui_button_color"];
		state.ui_button_color	   = ImVec4(arr[0], arr[1], arr[2], arr[3]);
		state.temp_ui_button_color = state.ui_button_color;
	}

	if (j.contains("ui_input_color") && j["ui_input_color"].is_array())
	{
		auto arr				  = j["ui_input_color"];
		state.ui_input_color	  = ImVec4(arr[0], arr[1], arr[2], arr[3]);
		state.temp_ui_input_color = state.ui_input_color;
	}

	switch (state.selected_theme)
	{
	case 0:
		ImGui::StyleColorsDark();
		break;
	case 1:
		ImGui::StyleColorsLight();
		break;
	case 2:
		ImGui::StyleColorsClassic();
		break;
	}

	apply_ui_colors(state);
}

int normalize_audio_files_in_directory(const std::string& input_dir, AppState& state)
{
	if (!fs::exists(input_dir) || !fs::is_directory(input_dir))
	{
		state.error_message = "Invalid or non-existent directory";
		state.show_error	= true;
		return 1;
	}

	state.processed_files = 0;
	state.log_lines.clear();

	// filter chain
	const std::string filter_chain = "loudnorm=I=-14:TP=-1.5:LRA=11,"
									 "acompressor=threshold=-20dB:ratio=4:attack=5:release=200:makeup=6,"
									 "alimiter=limit=1.0";

	for (const auto& entry : fs::directory_iterator(input_dir))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		auto ext = entry.path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext != ".wav" && ext != ".mp3" && ext != ".flac")
		{
			continue;
		}

		const auto& path		= entry.path();
		std::string temp_output = path.parent_path().string() + "/temp_" + path.filename().string();
		std::string command		= "ffmpeg -y -i \"" + path.string() + "\" -af \"" + filter_chain + "\" \"" + temp_output + "\"";

		int result = std::system(command.c_str());
		std::error_code ec;

		if (result == 0)
		{
			fs::remove(path, ec);
			if (ec)
			{
				spdlog::error("Error removing original file {}: {}", path.filename().string(), ec.message());
				fs::remove(temp_output, ec);
				state.log_lines.emplace_back(path.filename().string(), false);
				continue;
			}

			fs::rename(temp_output, path, ec);
			if (!ec)
			{
				++state.processed_files;
				state.log_lines.emplace_back(path.filename().string(), true);
			}
			else
			{
				spdlog::error("Error renaming temp file {}: {}", temp_output, ec.message());
				state.log_lines.emplace_back(path.filename().string(), false);
			}
		}
		else
		{
			spdlog::error("FFmpeg failed on {}: exit code {}", path.filename().string(), result);
			fs::remove(temp_output, ec);
			state.log_lines.emplace_back(path.filename().string(), false);
		}
		spdlog::info("Processed: {}", path.filename().string());
	}

	spdlog::info("Processing completed. Files processed: {}", state.processed_files);
	return 0;
}

void render_ui(AppState& state)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("Audio Normalizer", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

	constexpr float kCursorOffsetX = 10.0F;
	constexpr float kCursorOffsetY = 130.0F;
	ImGui::SetCursorPosX(kCursorOffsetX);

	ImGui::BeginGroup();

	ImGui::Text("Theme Settings");
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Theme:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	const char* themes[] = {"Dark", "Light", "Classic"};
	if (ImGui::Combo("##ThemeSelector", &state.selected_theme, themes, IM_ARRAYSIZE(themes)))
	{
		switch (state.selected_theme)
		{
		case 0:
			ImGui::StyleColorsDark();
			break;
		case 1:
			ImGui::StyleColorsLight();
			break;
		case 2:
			ImGui::StyleColorsClassic();
			break;
		}
		apply_ui_colors(state);
	}

	ImGui::Spacing();

	if (ImGui::Button("Save Theme", ImVec2(100, 25)))
	{
		save_settings(state);
	}
	ImGui::SameLine();
	ImGui::SetCursorPosX(120);
	if (ImGui::Button("Pick BG Color", ImVec2(100, 25)))
	{
		ImVec2 btn_min			= ImGui::GetItemRectMin();
		ImVec2 btn_max			= ImGui::GetItemRectMax();
		state.color_picker_pos	= ImVec2(btn_min.x, btn_max.y);
		state.show_color_picker = !state.show_color_picker;
		if (state.show_color_picker)
		{
			state.temp_color = state.custom_color;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Pick UI Colors", ImVec2(100, 25)))
	{
		ImVec2 btn_min			   = ImGui::GetItemRectMin();
		ImVec2 btn_max			   = ImGui::GetItemRectMax();
		state.ui_color_picker_pos  = ImVec2(btn_min.x, btn_max.y);
		state.show_ui_color_picker = !state.show_ui_color_picker;
		if (state.show_ui_color_picker)
		{
			state.temp_ui_button_color = state.ui_button_color;
			state.temp_ui_input_color  = state.ui_input_color;
		}
	}

	ImGui::Spacing();
	if (state.is_saving)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.8F, 0.8F, 0.2F, 1.0F), "Saving in progress...");
	}
	else if (state.save_completed)
	{
		auto now	  = std::chrono::steady_clock::now();
		float elapsed = std::chrono::duration<float>(now - state.save_completed_time).count();
		if (elapsed < 2.0f)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.2F, 0.8F, 0.2F, 1.0F), "saved successfully!");
		}
		else
		{
			state.save_completed = false;
		}
	}
	ImGui::Spacing();

	if (state.show_color_picker)
	{
		ImGui::SetNextWindowPos(state.color_picker_pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(300, 220), ImGuiCond_Always);

		ImGui::Begin("Background Color Picker", &state.show_color_picker, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

		float window_width = ImGui::GetWindowSize().x;
		float text_width   = ImGui::CalcTextSize("Background Color").x;
		ImGui::SetCursorPosX((window_width - text_width) * 0.5F);
		ImGui::Text("Background Color");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		const float picker_width  = 200.0F;
		const float picker_height = 120.0F;
		ImGui::SetCursorPosX((window_width - picker_width) * 0.5F);

		ImVec4 display_color = reduce_opacity(state.temp_color);
		ImGui::ColorPicker4("##bgcolorpicker", (float*)&state.temp_color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel,
							(float*)(float[2]){picker_width, picker_height});

		ImGui::Spacing();
		ImGui::Text("Preview (with 20%% opacity reduction):");
		ImGui::ColorButton("##preview", display_color, 0, ImVec2(50, 20));

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float button_width		 = 80.0F;
		float button_spacing	 = 20.0F;
		float total_button_width = (button_width * 2) + button_spacing;
		float start_x			 = (window_width - total_button_width) * 0.5F;

		ImGui::SetCursorPosX(start_x);
		if (ImGui::Button("Apply", ImVec2(button_width, 30)))
		{
			state.custom_color		= state.temp_color;
			state.show_color_picker = false;
			save_settings(state);
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(start_x + button_width + button_spacing);
		if (ImGui::Button("Cancel", ImVec2(button_width, 30)))
		{
			state.show_color_picker = false;
		}

		ImGui::End();
	}

	if (state.show_ui_color_picker)
	{
		ImGui::SetNextWindowPos(state.ui_color_picker_pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_Always);

		ImGui::Begin("UI Colors Picker", &state.show_ui_color_picker, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

		float window_width = ImGui::GetWindowSize().x;
		float text_width   = ImGui::CalcTextSize("Interface Colors").x;
		ImGui::SetCursorPosX((window_width - text_width) * 0.5F);
		ImGui::Text("Interface Colors");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Button Color:");
		ImVec4 button_preview = reduce_opacity(state.temp_ui_button_color);
		ImGui::ColorPicker4("##buttoncolorpicker", (float*)&state.temp_ui_button_color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel,
							(float*)(float[2]){150.0F, 80.0F});

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::Text("Preview:");
		ImGui::ColorButton("##buttonpreview", button_preview, 0, ImVec2(40, 20));
		ImGui::EndGroup();

		ImGui::Spacing();

		ImGui::Text("Input Field Color:");
		ImVec4 input_preview = reduce_opacity(state.temp_ui_input_color);
		ImGui::ColorPicker4("##inputcolorpicker", (float*)&state.temp_ui_input_color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel,
							(float*)(float[2]){150.0F, 80.0F});

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::Text("Preview:");
		ImGui::ColorButton("##inputpreview", input_preview, 0, ImVec2(40, 20));
		ImGui::EndGroup();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float button_width		 = 80.0F;
		float button_spacing	 = 20.0F;
		float total_button_width = (button_width * 2) + button_spacing;
		float start_x			 = (window_width - total_button_width) * 0.5F;

		ImGui::SetCursorPosX(start_x);
		if (ImGui::Button("Apply", ImVec2(button_width, 30)))
		{
			state.ui_button_color = state.temp_ui_button_color;
			state.ui_input_color  = state.temp_ui_input_color;
			apply_ui_colors(state);
			state.show_ui_color_picker = false;
			save_settings(state);
		}

		ImGui::SameLine();
		ImGui::SetCursorPosX(start_x + button_width + button_spacing);
		if (ImGui::Button("Cancel", ImVec2(button_width, 30)))
		{
			state.show_ui_color_picker = false;
		}

		ImGui::End();
	}

	ImGui::EndGroup();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Text("Audio File Processing");
	ImGui::Text("Enter the path to the folder containing the audio files");
	ImGui::Separator();
	ImGui::Spacing();

	float total_width  = ImGui::GetContentRegionAvail().x;
	float button_width = 120.0F;
	float input_width  = total_width - button_width - 20.0F;

	float content_width = input_width + button_width + 10.0F;
	ImGui::SetCursorPosX((total_width - content_width) * 0.5F);

	ImGui::PushItemWidth(input_width);
	ImGui::InputText("##path", state.path_buffer, sizeof(state.path_buffer), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	ImGui::SameLine();

	if (ImGui::Button("Browse Folder", ImVec2(button_width, 0)))
	{
		state.file_browser.SetTitle("select audio directory");
		state.file_browser.SetTypeFilters({});
		state.file_browser.Open();
	}

	state.file_browser.Display();

	if (state.file_browser.HasSelected())
	{
		std::filesystem::path selected = state.file_browser.GetSelected();

		if (std::filesystem::is_regular_file(selected))
		{
			selected = selected.parent_path();
		}

		std::string path_str = selected.string();
		strncpy(state.path_buffer, path_str.c_str(), sizeof(state.path_buffer) - 1);
		state.path_buffer[sizeof(state.path_buffer) - 1] = '\0';
		state.selected_path								 = path_str;
		state.show_success								 = false;
		state.show_error								 = false;

		state.file_browser.ClearSelected();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (!state.selected_path.empty())
	{
		ImGui::TextColored(ImVec4(0.7F, 0.9F, 1.0F, 1.0F), "Selected Directory:");
		ImGui::Indent();
		ImGui::TextWrapped("%s", state.selected_path.c_str());
		ImGui::Unindent();

		ImGui::Spacing();

		if (fs::exists(state.selected_path) && fs::is_directory(state.selected_path))
		{
			ImGui::TextColored(ImVec4(0.2F, 0.8F, 0.2F, 1.0F), "Valid directory");

			int audio_files = 0;
			try
			{
				for (const auto& entry : fs::directory_iterator(state.selected_path))
				{
					if (entry.is_regular_file())
					{
						auto ext = entry.path().extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						if (ext == ".wav" || ext == ".mp3" || ext == ".flac")
						{
							audio_files++;
						}
					}
				}
				ImGui::Text("Audio files found: %d", audio_files);
			}
			catch (...)
			{
				ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.2F, 1.0F), "Warning: Unable to read folder");
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Directory not found");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	ImGui::Spacing();

	float btn_norm_width = 220.0f;
	ImGui::SetCursorPosX((total_width - btn_norm_width) * 0.5f);

	bool can_process = !state.selected_path.empty() && fs::exists(state.selected_path) && fs::is_directory(state.selected_path) && !state.processing;

	if (!can_process)
	{
		ImGui::BeginDisabled();
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

	if (ImGui::Button("Normalize Audio Files", ImVec2(btn_norm_width, 45)))
	{
		state.processing   = true;
		state.show_success = false;
		state.show_error   = false;

		std::thread(
			[&state]()
			{
				int result		 = normalize_audio_files_in_directory(state.selected_path, state);
				state.processing = false;
				if (result == 0)
					state.show_success = true;
			})
			.detach();
	}

	ImGui::PopStyleColor(3);

	if (!can_process)
		ImGui::EndDisabled();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Separator();

	ImGui::BeginGroup();
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Information");
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Bullet();
	ImGui::Text("Supported formats: WAV, MP3, FLAC");
	ImGui::Bullet();
	ImGui::Text("Requires FFmpeg installed on the system");
	ImGui::Bullet();
	ImGui::Text("Tip: Copy and paste the path from file manager");
	ImGui::EndGroup();

	ImGui::Separator();

	if (state.processing)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float processing_text_width = ImGui::CalcTextSize("Processing in progress...").x;
		ImGui::SetCursorPosX((total_width - processing_text_width) * 0.5F);
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0F), "Processing in progress...");

		ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-1.0F, 0.0F));

		float tip_text_width = ImGui::CalcTextSize("This may take several minutes...").x;
		ImGui::SetCursorPosX((total_width - tip_text_width) * 0.5F);
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0F), "This may take several minutes...");
	}

	if (state.show_success)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float success_text_width = ImGui::CalcTextSize("Processing completed successfully!").x;
		ImGui::SetCursorPosX((total_width - success_text_width) * 0.5F);
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Processing completed successfully!");

		float files_text = ImGui::CalcTextSize("Files processed: 999").x;
		ImGui::SetCursorPosX((total_width - files_text) * 0.5f);
		ImGui::Text("Files processed: %d", state.processed_files);
	}

	if (!state.log_lines.empty())
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.8F, 0.8F, 0.8F, 1.0F), "Processing Status:");
		ImGui::Spacing();

		if (ImGui::BeginChild("LogArea", ImVec2(0, 150), 1))
		{
			for (const auto& log : state.log_lines)
			{
				const auto& filename = log.first;
				bool success		 = log.second;

				ImVec4 color = success ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.9f, 0.2f, 0.2f, 1.0F);
				ImGui::TextColored(color, "%s %s", filename.c_str());
			}
		}
		ImGui::EndChild();
	}

	if (state.show_error)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.9F, 0.2F, 0.2F, 1.0F), "Error:");
		ImGui::Indent();
		ImGui::TextWrapped("%s", state.error_message.c_str());
		ImGui::Unindent();
	}

	ImGui::End();
}

void glfw_error_callback(int error, const char* description)
{
	spdlog::error("GLFW Error {}: {}", error, description);
}

auto main(int argc, char* argv[]) -> int
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		spdlog::error("Error initializing GLFW");
		return 1;
	}

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "Audio Normalizer", nullptr, nullptr);
	if (window == nullptr)
	{
		spdlog::error("Error creating window");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		spdlog::error("Error initializing GLAD");
		glfwTerminate();
		return 1;
	}

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	std::cout << "Working dir: " << std::filesystem::current_path() << std::endl;
	io.Fonts->AddFontFromFileTTF("fonts/static/Roboto-Regular.ttf", 16.0F);
	ImGui::StyleColorsDark();
	AppState app_state;

	load_settings(app_state);

	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);


	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		render_ui(app_state);

		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);

		ImVec4 bg_color = reduce_opacity(app_state.custom_color);
		glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}