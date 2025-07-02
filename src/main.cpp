#include "../lib/gl3w/gl3w.h"
#include "../lib/imgui/backends/imgui_impl_opengl3.h"
#include "../lib/imgui/backends/imgui_impl_sdl2.h"
#include "../lib/imgui/imgui.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using json = nlohmann::json;

float g_price_now  = 0.0F;
float g_price_high = 0.0F;
std::string g_api_key;
std::chrono::time_point<std::chrono::steady_clock> g_last_fetch;

char g_crypto_id[64]	= "bitcoin";
char g_input_crypto[64] = "";
std::vector<std::string> g_crypto_watchlist;
std::map<std::string, float> g_prices;

size_t curl_write(void* contents, size_t size, size_t nmemb, std::string* output)
{
	output->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string read_api_key()
{
	const char* cmd = "gpg --quiet --batch --yes --decrypt config/apikey.txt.gpg 2>/dev/null";
	FILE* pipe		= popen(cmd, "r");
	if (!pipe)
	{
		std::cerr << "Failed to run gpg command. \n";
		return "";
	}

	std::ostringstream key_stream;
	char buffer[128];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
	{
		key_stream << buffer;
	}
	pclose(pipe);

	return key_stream.str();
}

void fetch_watchlist_prices()
{
	if (g_crypto_watchlist.empty())
	{
		return;
	}

	std::string ids;
	for (const auto& id : g_crypto_watchlist)
	{
		if (!ids.empty())
		{
			ids += ",";
		}
		ids += id;
	}

	std::string url = "https://api.coingecko.com/api/v3/simple/price?ids=" + ids + "&vs_currencies=usd";

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		return;
	}

	std::string response;

	struct curl_slist* headers = nullptr;
	std::string auth_header	   = "x-cg-demo_api-key: " + g_api_key;
	headers					   = curl_slist_append(headers, auth_header.c_str());

	std::cout << "URL: " << url << "\n";

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK)
	{
		try
		{
			auto parsed = json::parse(response);
			for (const auto& id : g_crypto_watchlist)
			{
				if (parsed.contains(id) && parsed[id].contains("usd"))
				{
					g_prices[id] = parsed[id]["usd"].get<float>();
				}
			}
		}
		catch (std::exception& e)
		{
			std::cerr << "JSON parsing error: " << e.what() << "\nResponse: " << response << "\n";
		}
	}
	else
	{
		std::cerr << "CURL error: " << curl_easy_strerror(res) << "\n";
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

int main(int, char**)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	g_api_key = read_api_key();
	if (g_api_key.empty())
	{
		std::cerr << "failed to load api key \n";
		return -1;
	}

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window* window		 = SDL_CreateWindow("Bitcoin Tracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	SDL_GL_SetSwapInterval(1);

	if (gl3wInit() != 0)
	{
		std::cerr << "Failed to initialize OpenGL loader (gl3w)\n";
		return -1;
	}

	gl3wInit();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 130");

	ImGui::StyleColorsDark();

	g_last_fetch = std::chrono::steady_clock::now() - std::chrono::seconds(10);

	bool running = true;
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
		}

		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - g_last_fetch).count() >= 5)
		{
			fetch_watchlist_prices();
			g_last_fetch = now;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io.DisplaySize);
		ImGui::Begin("Bitcoin Info", nullptr,
					 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

		ImGui::SetCursorPos(ImVec2(20, 20));
		ImGui::Text("Add crypto: ");

		ImGui::SameLine();
		ImGui::InputText("##crypto_input", g_input_crypto, sizeof(g_input_crypto));

		ImGui::SameLine();
		if (ImGui::Button("Add"))
		{
			std::string crypto = g_input_crypto;
			std::transform(crypto.begin(), crypto.end(), crypto.begin(), ::tolower);
			if (!crypto.empty() && std::find(g_crypto_watchlist.begin(), g_crypto_watchlist.end(), crypto) == g_crypto_watchlist.end())
			{
				g_crypto_watchlist.push_back(crypto);
				g_input_crypto[0] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("observing crypto: ");

		for (auto it = g_crypto_watchlist.begin(); it != g_crypto_watchlist.end();)
		{
			const std::string& id = *it;
			float price			  = g_prices.count(id) ? g_prices[id] : 0.0F;

			ImGui::BulletText("%s: $%.2F", id.c_str(), price);
			ImGui::SameLine();
			std::string btn_id = "Remove##" + id;
			if (ImGui::Button(btn_id.c_str()))
			{
				it = g_crypto_watchlist.erase(it);
				continue;
			}
			++it;
		}

		ImGui::End();

		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);
	}

	std::system("gpgconf --kill gpg-agent");

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	curl_global_cleanup();

	return 0;
}