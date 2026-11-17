#pragma once

struct GLFWwindow;

class GameFrameWork
{
public:
	int Run();
private:
	bool Init();
	void Update(float deltaTime);
	void Draw();
	void Clear();

	GLFWwindow* window = nullptr;
};