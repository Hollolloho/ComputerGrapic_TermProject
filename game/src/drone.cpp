#include "drone.h"
#include "Shader.h"
#include "model_loader.h"

#include <iostream>

bool Drone::Init()
{
	Clear();

	aegis::ModelData model;
	std::string error;
	const std::filesystem::path path = "asset/drone/obj/drone_assembled.obj";
	if (!aegis::LoadModel(path, model, error))
	{
		std::cerr << error << '\n';
		return false;
	}

	shaderProgram = LoadShaderProgram("shaders/drone.vert", "shaders/drone.frag");
	if (!shaderProgram)
		return false;

	transformLocation = glGetUniformLocation(shaderProgram, "transform");
	colorLocation = glGetUniformLocation(shaderProgram, "baseColor");
	emissionLocation = glGetUniformLocation(shaderProgram, "emissionColor");

	if (!UploadMeshes(model, meshes))
	{
		std::cerr << "Could not upload meshes: " << path << '\n';
		Clear();
		return false;
	}
	return true;
}

void Drone::Clear()
{
	ClearMeshes(meshes);

	if (shaderProgram)
	{
		glUseProgram(0);
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}
	transformLocation = -1;
	colorLocation = -1;
	emissionLocation = -1;
}
