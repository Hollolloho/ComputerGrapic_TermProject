#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
	GLuint CompileShader(GLenum type, const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file)
		{
			std::cerr << "Could not open shader: " << path << '\n';
			return 0;
		}

		std::ostringstream buffer;
		buffer << file.rdbuf();
		const std::string source = buffer.str();
		const char* text = source.c_str();
		const GLuint shader = glCreateShader(type);
		if (!shader)
			return 0;

		glShaderSource(shader, 1, &text, nullptr);
		glCompileShader(shader);

		GLint success = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success != GL_TRUE)
		{
			char log[1024]{};
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			std::cerr << path << ": " << log << '\n';
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}
}

GLuint LoadShaderProgram(const std::filesystem::path& vertexPath,
	const std::filesystem::path& fragmentPath)
{
	const GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexPath);
	if (!vertex)
		return 0;

	const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentPath);
	if (!fragment)
	{
		glDeleteShader(vertex);
		return 0;
	}

	const GLuint program = glCreateProgram();
	if (program)
	{
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
	}
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	if (!program)
		return 0;

	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success != GL_TRUE)
	{
		char log[1024]{};
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		std::cerr << "Shader link failed (" << vertexPath << ", " << fragmentPath << "): " << log << '\n';
		glDeleteProgram(program);
		return 0;
	}
	return program;
}
