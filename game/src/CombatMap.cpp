#include "CombatMap.h"
#include "Shader.h"
#include "model_loader.h"
#include "combat_map_layout.generated.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

bool CombatMap::Init(const std::filesystem::path& projectRoot)
{
	Clear();

	aegis::ModelData model;

	std::string error;
	const auto path = projectRoot / aegis::combat_map_asset::mesh;
    const aegis::ObjModelCache cache(projectRoot / "build/cache/models",
        projectRoot / "asset/combat_map/obj/combat_map.mtl");
    bool cacheHit{};
    if (!cache.Load(path, model, error, cacheHit)) {
        std::cerr << error << '\n';
        return false;
    }
    std::size_t triangles{};
    std::vector<aegis::ModelMesh> batches(model.materials.size());
    for (const auto& source : model.meshes) {
        if (source.materialIndex >= batches.size()) return false;
        triangles += source.indices.size() / 3;
        auto& batch = batches[source.materialIndex];
        const auto offset = static_cast<unsigned int>(batch.vertices.size());
        batch.vertices.insert(batch.vertices.end(), source.vertices.begin(), source.vertices.end());
        for (auto index : source.indices) batch.indices.push_back(index + offset);
    }
    if (triangles != aegis::combat_map_asset::triangles) {
        std::cerr << "Combat map collision layout is out of date\n";
        return false;
    }
    shaderProgram = LoadShaderProgram(projectRoot/"game/shaders/combat_map.vert",projectRoot/"game/shaders/combat_map.frag");

	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");

    if (transformLocation < 0 || colorLocation < 0 || emissionLocation < 0) {
        Clear();
        return false;
    }
    aegis::ModelData batched;
    batched.materials=std::move(model.materials);
    for(std::size_t i=0;i<batches.size();++i) {
        if(batches[i].indices.empty()) continue;
        batches[i].materialIndex=static_cast<unsigned int>(i);
        batched.meshes.push_back(std::move(batches[i]));
    }
    if(!UploadMeshes(batched,meshes)) { Clear();return false; }
    std::cout << "Combat map: meshes=" << meshes.size() << " triangles=" << triangles << " cache=" << cacheHit << '\n';
	return true;
}

void CombatMap::Draw(const glm::mat4& scene) const
{
	if (!shaderProgram) return;
	// 맵 OBJ는 월드 위치로 제작돼 모델 행렬은 단위 행렬이다.
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(scene));
	for (const RenderMesh& mesh : meshes)
	{
		glUniform3fv(colorLocation, 1, glm::value_ptr(mesh.color));
		glUniform3fv(emissionLocation, 1, glm::value_ptr(mesh.emission));
		glBindVertexArray(mesh.vao);
		glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
	}
	glBindVertexArray(0);
    glUseProgram(0);
}

void CombatMap::Clear()
{
    ClearMeshes(meshes);

	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
}
