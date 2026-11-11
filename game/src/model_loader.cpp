#include "model_loader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <utility>

using namespace std;

// static 함수는 이 파일 안에서만 사용한다.
static glm::vec3 ReadColor(const aiMaterial& material, const char* key, unsigned int type,
	unsigned int index, const glm::vec3& fallback);
static ModelMaterial ReadMaterial(const aiMaterial& source);

bool LoadModel(const filesystem::path& path, ModelData& model, string& error)
{
	// Assimp에는 UTF-8 경로를 전달한다.
	u8string utf8Path = path.u8string();
	string fileName(utf8Path.begin(), utf8Path.end());
	unsigned int importFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		aiProcess_GenNormals | aiProcess_ValidateDataStructure;

	// importer가 원본 데이터를 관리하고, 함수가 끝나면 자동으로 정리한다.
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(fileName, importFlags);
	if (!scene)
	{
		error = fileName + ": " + importer.GetErrorString();
		return false;
	}

	// 필요한 값만 복사한다. 로딩에 모두 성공한 뒤 model에 넘긴다.
	ModelData loaded;
	loaded.materials.reserve(scene->mNumMaterials);
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
	{
		loaded.materials.push_back(ReadMaterial(*scene->mMaterials[i]));
	}

	loaded.meshes.reserve(scene->mNumMeshes);
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh& source = *scene->mMeshes[i];
		ModelMesh mesh;
		mesh.name = source.mName.C_Str();
		mesh.materialIndex = source.mMaterialIndex;
		mesh.hasUvs = source.HasTextureCoords(0);
		mesh.indices.reserve(static_cast<size_t>(source.mNumFaces) * 3);

		for (unsigned int faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex)
		{
			const aiFace& face = source.mFaces[faceIndex];
			if (face.mNumIndices == 3)
			{
				mesh.indices.insert(mesh.indices.end(), face.mIndices, face.mIndices + 3);
			}
		}
		if (mesh.indices.empty())
		{
			continue;
		}

		bool hasNormals = source.HasNormals();
		mesh.vertices.reserve(source.mNumVertices);
		for (unsigned int vertexIndex = 0; vertexIndex < source.mNumVertices; ++vertexIndex)
		{
			ModelVertex vertex;
			const aiVector3D& position = source.mVertices[vertexIndex];
			vertex.position = { position.x, position.y, position.z };
			if (hasNormals)
			{
				const aiVector3D& normal = source.mNormals[vertexIndex];
				vertex.normal = { normal.x, normal.y, normal.z };
			}
			if (mesh.hasUvs)
			{
				const aiVector3D& uv = source.mTextureCoords[0][vertexIndex];
				vertex.uv = { uv.x, uv.y };
			}
			mesh.vertices.push_back(vertex);
		}
		loaded.meshes.push_back(move(mesh));
	}

	if (loaded.meshes.empty())
	{
		error = fileName + ": no triangle meshes";
		return false;
	}

	model = move(loaded);
	error.clear();
	return true;
}

// ===== MTL 재질 정보 복사 =====

static glm::vec3 ReadColor(const aiMaterial& material, const char* key, unsigned int type,
	unsigned int index, const glm::vec3& fallback)
{
	aiColor3D color;
	if (material.Get(key, type, index, color) == AI_SUCCESS)
	{
		return { color.r, color.g, color.b };
	}
	return fallback;
}

static ModelMaterial ReadMaterial(const aiMaterial& source)
{
	ModelMaterial material;
	material.name = source.GetName().C_Str();
	material.ambient = ReadColor(source, AI_MATKEY_COLOR_AMBIENT, material.ambient);
	material.diffuse = ReadColor(source, AI_MATKEY_COLOR_DIFFUSE, material.diffuse);
	material.specular = ReadColor(source, AI_MATKEY_COLOR_SPECULAR, material.specular);
	material.emissive = ReadColor(source, AI_MATKEY_COLOR_EMISSIVE, material.emissive);
	source.Get(AI_MATKEY_SHININESS, material.shininess);
	source.Get(AI_MATKEY_OPACITY, material.opacity);

	aiString texture;
	if (source.GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS)
	{
		material.diffuseTexture = texture.C_Str();
	}
	return material;
}

// ===== OBJ 데이터 확인 명령 =====

static int InspectModel(const filesystem::path& path)
{
	ModelData model;
	string error;
	if (!LoadModel(path, model, error))
	{
		cerr << error << '\n';
		return 1;
	}

	size_t vertices{};
	size_t triangles{};
	for (const ModelMesh& mesh : model.meshes)
	{
		vertices += mesh.vertices.size();
		triangles += mesh.indices.size() / 3;
	}
	cout << "meshes=" << model.meshes.size() << " vertices=" << vertices
		<< " triangles=" << triangles << " materials=" << model.materials.size() << '\n';

	for (const ModelMaterial& material : model.materials)
	{
		cout << material.name << " Kd=" << material.diffuse.r << ',' << material.diffuse.g
			<< ',' << material.diffuse.b << " opacity=" << material.opacity;
		if (!material.diffuseTexture.empty())
		{
			cout << " map_Kd=" << material.diffuseTexture;
		}
		cout << '\n';
	}
	return 0;
}

int RunModelInspection(int argc, char* argv[])
{
	if (argc == 3 && string(argv[1]) == "--inspect-obj")
	{
		return InspectModel(argv[2]);
	}
	cerr << "Usage: AEGIS.exe [--inspect-obj <model.obj>]\n";
	return 2;
}
