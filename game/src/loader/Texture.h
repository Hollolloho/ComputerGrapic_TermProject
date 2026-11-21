#pragma once

#include <GL/glew.h>
#include <filesystem>

GLuint LoadPngTexture(const std::filesystem::path& path, bool srgb, GLint wrapMode);
