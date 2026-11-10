#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct ModelVertex
{
	glm::vec3 position{ 0.0f };
	glm::vec3 normal{ 0.0f };
	glm::vec2 uv{ 0.0f };
};

struct ModelMesh
{
	std::string name;
	std::vector<ModelVertex> vertices;
	std::vector<unsigned int> indices;
	unsigned int materialIndex{};
	bool hasUvs{};
};

struct ModelMaterial
{
	std::string name;
	glm::vec3 ambient{ 0.0f };
	glm::vec3 diffuse{ 1.0f };
	glm::vec3 specular{ 0.0f };
	glm::vec3 emissive{ 0.0f };
	float shininess{};
	float opacity{ 1.0f };
	std::string diffuseTexture;
};

struct ModelData
{
	std::vector<ModelMesh> meshes;
	std::vector<ModelMaterial> materials;
};

// 결과는 Assimp 장면과 독립적이다. 읽기에 실패하면 기존 model을 유지한다.
bool LoadModel(const std::filesystem::path& path, ModelData& model, std::string& error);
