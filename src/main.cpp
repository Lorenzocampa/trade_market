#include "../include/main.hpp"

#include "spdlog/spdlog.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

auto normalize_audio_files_in_directory(const std::string& input_dir) -> int
{
	if (!fs::exists(input_dir) || !fs::is_directory(input_dir))
	{
		return 1;
	}

	for (const auto& entry : fs::directory_iterator(input_dir))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		const auto& path = entry.path();
		auto ext		 = path.extension().string();

		if (ext != ".wav" && ext != ".mp3" && ext != ".flac")
		{
			continue;
		}

		std::string temp_output = path.parent_path().string() + "/temp_" + path.filename().string();
		std::string command		= "ffmpeg -y -i \"" + path.string() + "\" -af loudnorm \"" + temp_output + "\"";

		if (std::system(command.c_str()) == 0)
		{
			spdlog::info("running command: {}", command);
			std::error_code ec;
			fs::remove(path, ec);

			if (ec)
			{
				fs::remove(temp_output, ec);
				continue;
			}

			fs::rename(temp_output, path, ec);
		}
		else
		{
			{
				std::error_code ec;
				fs::remove(temp_output, ec);
			}
		}

		spdlog::debug("all audio files normalized in place");
	}
	return 0;
}

auto main(int argc, char* argv[]) -> int
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <directory_with_audio_files>\n";
		return 1;
	}

	return normalize_audio_files_in_directory(argv[1]);
}
