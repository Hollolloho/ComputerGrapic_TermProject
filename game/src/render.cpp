#include "game.h"

#include <glm/gtc/matrix_transform.hpp>

bool Render(const Game& game)
{
	int width{}, height{};
	glfwGetFramebufferSize(game.window, &width, &height);
	if (width <= 0 || height <= 0)
	{
		return false;
	}

	const Cube& cube = game.cube;
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	const float aspect = static_cast<float>(width) / height;
	const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	const glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(22.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(-35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 transform = projection * view * model;

	glUseProgram(cube.program);
	glUniformMatrix4fv(cube.transformLocation, 1, GL_FALSE, &transform[0][0]);
	glBindVertexArray(cube.vertexArray);
	glDrawElements(GL_TRIANGLES, cube.indexCount, GL_UNSIGNED_INT, nullptr);
	glfwSwapBuffers(game.window);
	return true;
}
