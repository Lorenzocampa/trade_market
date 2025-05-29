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
#include <iostream>
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

	ImGui::FileBrowser file_browser{ImGuiFileBrowserFlags_SelectDirectory};
};

auto normalize_audio_files_in_directory(const std::string& input_dir, AppState& state) -> int
{
	if (!fs::exists(input_dir) || !fs::is_directory(input_dir))
	{
		state.error_message = "Directory non valida o inesistente";
		state.show_error	= true;
		return 1;
	}

	state.processed_files = 0;

	for (const auto& entry : fs::directory_iterator(input_dir))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		const auto& path = entry.path();
		auto ext		 = path.extension().string();

		// Converti l'estensione in lowercase per il confronto
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext != ".wav" && ext != ".mp3" && ext != ".flac")
		{
			continue;
		}

		std::string temp_output = path.parent_path().string() + "/temp_" + path.filename().string();
		std::string command		= "ffmpeg -y -i \"" + path.string() + "\" -af loudnorm \"" + temp_output + "\"";

		spdlog::info("Elaborando: {}", path.filename().string());

		if (std::system(command.c_str()) == 0)
		{
			std::error_code ec;
			fs::remove(path, ec);
			if (ec)
			{
				spdlog::error("Errore nella rimozione del file originale: {}", ec.message());
				fs::remove(temp_output, ec);
				continue;
			}
			fs::rename(temp_output, path, ec);
			if (!ec)
			{
				state.processed_files++;
			}
		}
		else
		{
			spdlog::error("Errore nell'elaborazione di: {}", path.filename().string());
			std::error_code ec;
			fs::remove(temp_output, ec);
		}
	}

	spdlog::info("Elaborazione completata. File processati: {}", state.processed_files);
	return 0;
}

void render_ui(AppState& state)
{
	// Finestra principale
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("Audio Normalizer", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Inserisci il percorso della cartella contenente i file audio");
	ImGui::Separator();

	// Centrare InputText + bottone sfoglia
	float total_width  = ImGui::GetContentRegionAvail().x;
	float button_width = 100.0f;
	float input_width  = total_width - button_width - 10.0f; // 10px di spazio tra input e bottone

	// Posiziona il blocco centrato
	ImGui::SetCursorPosX((total_width - (input_width + button_width + 10.0f)) * 0.5f);

	ImGui::PushItemWidth(input_width);
	ImGui::InputText("##path", state.path_buffer, sizeof(state.path_buffer), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	ImGui::SameLine();

	if (ImGui::Button("sfoglia", ImVec2(button_width, 0)))
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

	ImGui::Separator();

	// Bottone di esempio per percorsi comuni
	ImGui::Text("Percorsi di esempio:");

	// Centrare i 3 bottoni in linea con spaziatura
	float btn_example_width = 100.0f;
	float spacing			= 20.0f;
	float total_btn_width	= btn_example_width * 3 + spacing * 2;
	ImGui::SetCursorPosX((total_width - total_btn_width) * 0.5f);

	if (ImGui::Button("Home/Music", ImVec2(btn_example_width, 0)))
	{
		std::string home_music = std::string(getenv("HOME")) + "/Music";
		strcpy(state.path_buffer, home_music.c_str());
		state.selected_path = home_music;
	}
	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Button("Desktop", ImVec2(btn_example_width, 0)))
	{
		std::string desktop = std::string(getenv("HOME")) + "/Desktop";
		strcpy(state.path_buffer, desktop.c_str());
		state.selected_path = desktop;
	}
	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Button("Downloads", ImVec2(btn_example_width, 0)))
	{
		std::string downloads = std::string(getenv("HOME")) + "/Downloads";
		strcpy(state.path_buffer, downloads.c_str());
		state.selected_path = downloads;
	}

	ImGui::Separator();

	// Mostra il percorso selezionato
	if (!state.selected_path.empty())
	{
		ImGui::Text("Cartella selezionata:");
		ImGui::TextWrapped("%s", state.selected_path.c_str());

		if (fs::exists(state.selected_path) && fs::is_directory(state.selected_path))
		{
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Cartella valida");

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
				ImGui::Text("File audio trovati: %d", audio_files);
			}
			catch (...)
			{
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "Attenzione: impossibile leggere la cartella");
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Cartella non valida");
		}
		ImGui::Separator();
	}

	// Spaziatura sopra bottone normalizza
	ImGui::Dummy(ImVec2(0, 20));

	// Bottone Normalizza centrato
	float btn_norm_width = 200.0f;
	ImGui::SetCursorPosX((total_width - btn_norm_width) * 0.5f);

	bool can_process = !state.selected_path.empty() && fs::exists(state.selected_path) && fs::is_directory(state.selected_path) && !state.processing;

	if (!can_process)
		ImGui::BeginDisabled();

	if (ImGui::Button("Normalizza File Audio", ImVec2(btn_norm_width, 40)))
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

	if (!can_process)
		ImGui::EndDisabled();

	// Spaziatura sotto bottone normalizza
	ImGui::Dummy(ImVec2(0, 20));

	// Stato elaborazione
	if (state.processing)
	{
		ImGui::Separator();
		ImGui::Text("Elaborazione in corso...");
		ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-1.0f, 0.0f));
		ImGui::Text("Questo potrebbe richiedere diversi minuti...");
	}

	// Messaggi di successo
	if (state.show_success)
	{
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Elaborazione completata!");
		ImGui::Text("File processati: %d", state.processed_files);
	}

	// Messaggi di errore
	if (state.show_error)
	{
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ Errore:");
		ImGui::TextWrapped("%s", state.error_message.c_str());
	}

	ImGui::Separator();
	ImGui::Text("Formati supportati: WAV, MP3, FLAC");
	ImGui::Text("Richiede FFmpeg installato nel sistema");
	ImGui::Text("Suggerimento: copia e incolla il percorso dal file manager");

	ImGui::End();
}

void glfw_error_callback(int error, const char* description)
{
	spdlog::error("GLFW Error {}: {}", error, description);
}

auto main(int argc, char* argv[]) -> int
{
	// Setup GLFW
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		spdlog::error("Errore nell'inizializzazione di GLFW");
		return 1;
	}

	// Configurazione della finestra OpenGL
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Crea la finestra
	GLFWwindow* window = glfwCreateWindow(1024, 768, "Audio Normalizer", nullptr, nullptr);
	if (window == nullptr)
	{
		spdlog::error("Errore nella creazione della finestra");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Inizializza GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		spdlog::error("Errore nell'inizializzazione di GLAD");
		glfwTerminate();
		return 1;
	}

	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Stato dell'applicazione
	AppState app_state;

	// Main loop
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Render UI
		render_ui(app_state);

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}