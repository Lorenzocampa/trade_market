#ifndef MAIN_HPP
#define MAIN_HPP

#include "../lib/gl3w/gl3w.h"
#include "../lib/imgui/backends/imgui_impl_opengl3.h"
#include "../lib/imgui/backends/imgui_impl_sdl2.h"
#include "../lib/imgui/imgui.h"
#include "../lib/implot/implot.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <chrono>
#include <cstdlib>
#include <curl/curl.h>
#include <deque>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

float g_price_now  = 0.0F;
float g_price_high = 0.0F;
std::string g_api_key;
std::chrono::time_point<std::chrono::steady_clock> g_last_fetch;

char g_crypto_id[64]	= "bitcoin";
char g_input_crypto[64] = "";
std::vector<std::string> g_crypto_watchlist;
std::map<std::string, float> g_prices;

std::string g_focused_crypto = "";

struct PricePoint
{
	double timestamp;
	float price;
};

std::map<std::string, std::deque<PricePoint>> g_price_history_map;

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
	std::string auth_header	   = "x-cg-demo-api-key: " + g_api_key;
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

					double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();

					g_price_history_map[id].push_back({now, g_prices[id]});

					while (g_price_history_map[id].size() > 12096)
					{
						g_price_history_map[id].pop_front();
					}
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

bool fetch_crypto_history(const std::string& id, std::vector<double>& out_times, std::vector<double>& out_prices)
{
	std::string url = "https://api.coingecko.com/api/v3/coins/" + id + "/market_chart?vs_currency=usd&days=7";
	CURL* curl		= curl_easy_init();
	if (!curl)
	{
		return false;
	}

	std::string response;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	try
	{
		json parsed = json::parse(response);
		if (!parsed.contains("prices"))
		{
			return false;
		}
		for (auto& p : parsed["prices"])
		{
			out_times.push_back(p[0].get<double>() / 1000.0);
			out_prices.push_back(p[1].get<double>());
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

int format_timestamp(double value, char* buffer, int size, void*)
{
	std::time_t t = static_cast<std::time_t>(value);
	std::tm* tm	  = std::localtime(&t);
	if (tm)
	{
		return std::strftime(buffer, size, "%d %b", tm);
	}
	return 0;
}

float compute_rsi(const std::vector<double>& prices, size_t period = 14)
{
	if (prices.size() < period + 1)
	{
		return 0.0;
	}
	double gain = 0.0, loss = 0.0;
	for (size_t idx_for_i = prices.size() - period; idx_for_i < prices.size(); ++idx_for_i)
	{
		double delta = prices[idx_for_i + 1] - prices[idx_for_i];
		if (delta >= 0)
		{
			gain += delta;
		}
		else
		{
			loss -= delta;
		}
	}
	if (gain + loss == 0.0)
	{
		return 50.0;
	}
	double rs = gain / loss;
	return 100.0 - (100.0 / (1.0 + rs));
}

void compute_macd(const std::vector<double>& prices, double& macd, double& signal)
{
	if (prices.size() < 26)
	{
		macd = signal = 0.0;
		return;
	}

	double ema12   = prices[0];
	double ema26   = prices[0];
	double alpha12 = 2.0 / (12.0 + 1);
	double alpha26 = 2.0 / (26.0 + 1);
	for (size_t idx_for_i = 1; idx_for_i < prices.size(); ++idx_for_i)
	{
		ema12 = alpha12 * prices[idx_for_i] + (1 - alpha12) * ema12;
		ema26 = alpha26 * prices[idx_for_i] + (1 - alpha26) * ema26;
	}

	macd = ema12 - ema26;
	static std::vector<double> macd_history;
	macd_history.push_back(macd);
	if (macd_history.size() > 9)
	{
		macd_history.erase(macd_history.begin());
	}
	signal = std::accumulate(macd_history.begin(), macd_history.end(), 0.0) / macd_history.size();
}

void analyze_crypto(const std::vector<double>& times, const std::vector<double>& prices)
{
	float rsi	= compute_rsi(prices);
	double macd = 0.0, signal = 0.0;
	compute_macd(prices, macd, signal);

	ImGui::Text("RSI: %.2f%s", rsi, (rsi > 70 ? " (Overbought)" : (rsi < 30 ? " (Oversold)" : "")));
	ImGui::Text("MACD: %.4f", macd);
	ImGui::Text("Signal Line: %.4f", signal);
	if (macd > signal)
	{
		ImGui::Text("Signal: BUY (MACD > Signal)");
	}
	else if (macd < signal)
	{
		ImGui::Text("Signal: SELL (MACD < Signal)");
	}
	else
	{
		ImGui::Text("Signal: HOLD");
	}
}

void save_watchlist(const std::string& path)
{
	std::ofstream file(path);
	if (!file)
	{
		std::cerr << "Failed to save watchlist to " << path << "\n";
		return;
	}

	for (const auto& id : g_crypto_watchlist)
	{
		file << id << "\n";
	}
}

void load_watchlist(const std::string& path)
{

	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	if (!std::filesystem::exists(path))
	{
		std::ofstream create_file(path);
		if (!create_file)
		{
			std::cerr << "Failed to create watchlist file at " << path << "\n";
			return;
		}
		return;
	}

	std::ifstream file(path);
	std::string line;

	while (std::getline(file, line))
	{
		std::transform(line.begin(), line.end(), line.begin(), ::tolower);
		if (!line.empty() && std::find(g_crypto_watchlist.begin(), g_crypto_watchlist.end(), line) == g_crypto_watchlist.end())
			g_crypto_watchlist.push_back(line);
	}
}

#endif // MAIN_HPP
