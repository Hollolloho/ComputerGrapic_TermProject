#include "CombatMap.h"
#include "model_loader.h"

#include <cstddef>
#include <filesystem>
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
		GLuint shader = glCreateShader(type);
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

	GLuint MakeShaderProgram()
	{
		const GLuint vertex = CompileShader(GL_VERTEX_SHADER, "shaders/combat_map.vert");
		if (!vertex)
			return 0;

		const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, "shaders/combat_map.frag");
		if (!fragment)
		{
			glDeleteShader(vertex);
			return 0;
		}

		GLuint program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		glDeleteShader(vertex);
		glDeleteShader(fragment);

		GLint success = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (success != GL_TRUE)
		{
			char log[1024]{};
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			std::cerr << "Combat map shader link failed: " << log << '\n';
			glDeleteProgram(program);
			return 0;
		}
		return program;
	}
}

bool CombatMap::Init()
{
	Clear();

	aegis::ModelData model;

	std::string error;
	// 실행 작업 폴더를 기준으로 한 상대 경로.
	const std::filesystem::path path = "asset/combat_map/obj/combat_map.obj";

	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = MakeShaderProgram();

	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");

	for (const auto& source : model.meshes)
	{
		Mesh& mesh = meshes.emplace_back();
		mesh.indexCount = static_cast<GLsizei>(source.indices.size());
		mesh.color = model.materials[source.materialIndex].diffuse;
		mesh.emission = model.materials[source.materialIndex].emissive;

		glGenVertexArrays(1, &mesh.vao);
		glGenBuffers(1, &mesh.vbo);
		glGenBuffers(1, &mesh.ebo);

		glBindVertexArray(mesh.vao);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);

		glBufferData(GL_ARRAY_BUFFER, source.vertices.size() * sizeof(aegis::ModelVertex),
			source.vertices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
			reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, position)));

		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(aegis::ModelVertex),
			reinterpret_cast<const void*>(offsetof(aegis::ModelVertex, normal)));

		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, source.indices.size() * sizeof(unsigned int),
			source.indices.data(), GL_STATIC_DRAW);
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return true;
}

void CombatMap::Draw(const glm::mat4& scene) const
{
	// 맵 OBJ는 월드 위치로 제작돼 모델 행렬은 단위 행렬이다.
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(scene));
	for (const Mesh& mesh : meshes)
	{
		glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
		glBindVertexArray(mesh.vao);
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
	}
	glBindVertexArray(0);
}

void CombatMap::Clear()
{
	for (const Mesh& mesh : meshes)
	{
		glDeleteVertexArrays(1, &mesh.vao);
		glDeleteBuffers(1, &mesh.vbo);
		glDeleteBuffers(1, &mesh.ebo);
	}
	meshes.clear();

	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
}
