#include "game.h"
#include "model_loader.h"

int main(int argc, char* argv[])
{
	if (argc != 1)
	{
		return RunModelInspection(argc, argv);
	}

	Game game;
	if (!Setup(game))
	{
		return 1;
	}

	while (!glfwWindowShouldClose(game.window))
	{
		glfwPollEvents();
		Update(game.window);
		if (!Render(game))
		{
			glfwWaitEventsTimeout(0.1);
		}
	}

	Shutdown(game);
	return 0;
}
