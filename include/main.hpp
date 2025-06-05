#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>

int normalize_audio_files_in_directory(const std::string& input_dir);
void glfw_error_callback(int error, const char* description);

#endif // MAIN_HPP
