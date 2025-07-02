#include "../include/main.hpp"

int main(int, char**)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	g_api_key = read_api_key();
	if (g_api_key.empty())
	{
		std::cerr << "failed to load api key \n";
		return -1;
	}
	load_watchlist("config/watchlist.txt");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window* window		 = SDL_CreateWindow("Bitcoin Tracker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
	ImPlot::CreateContext();
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

			double now_sec = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

			for (const auto& [id, price] : g_prices)
			{
				auto& history = g_price_history_map[id];
				history.push_back({now_sec, price});

				if (history.size() > 12096)
				{
					history.pop_front();
				}
			}
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
			std::string btn_focus = "Focus##" + id;
			if (ImGui::Button(btn_focus.c_str()))
			{
				g_focused_crypto = id;
			}

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

		if (!g_focused_crypto.empty() && g_price_history_map.count(g_focused_crypto))
		{
			const auto& history = g_price_history_map[g_focused_crypto];

			std::vector<double> x_axis;
			std::vector<double> y_axis;
			for (const auto& pt : history)
			{
				x_axis.push_back(pt.timestamp);
				y_axis.push_back(pt.price);
			}

			if (ImPlot::BeginPlot("Price History", ImVec2(-1, 300)))
			{
				ImPlot::SetupAxes("Time", "USD");
				ImPlot::PlotLine(g_focused_crypto.c_str(), x_axis.data(), y_axis.data(), x_axis.size());
				ImPlot::EndPlot();
			}

			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(io.DisplaySize);
			ImGui::Begin("Crypto Details", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

			ImGui::SetCursorPos(ImVec2(0, 0));
			ImGui::Text("Details for: %s", g_focused_crypto.c_str());

			static std::string last_crypto;
			static std::vector<double> hist_times, hist_prices;
			static bool hist_loaded = false;
			if (last_crypto != g_focused_crypto)
			{
				hist_times.clear();
				hist_prices.clear();
				hist_loaded = fetch_crypto_history(g_focused_crypto, hist_times, hist_prices);
				last_crypto = g_focused_crypto;
			}

			if (hist_loaded && hist_times.size() > 1)
			{

				if (ImPlot::BeginPlot("Last 7 days", ImVec2(-1, 300)))
				{
					ImPlot::SetupAxes("Date", "USD", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
					ImPlot::SetupAxisFormat(ImAxis_X1, format_timestamp);
					ImPlot::PlotLine("USD", hist_times.data(), hist_prices.data(), hist_prices.size());
					ImPlot::EndPlot();
				}

				analyze_crypto(hist_times, hist_prices);
			}
			else
			{
				ImGui::Text("Loading history...");
			}

			float focused_price = g_prices.count(g_focused_crypto) ? g_prices[g_focused_crypto] : 0.0F;
			ImGui::Text("Actual Price: $%.2F", focused_price);

			ImGui::Spacing();
			if (ImGui::Button("Close"))
			{
				g_focused_crypto.clear();
			}

			ImGui::End();
		}

		ImGui::Render();
		save_watchlist("config/watchlist.txt");
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
	ImPlot::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	curl_global_cleanup();

	return 0;
}