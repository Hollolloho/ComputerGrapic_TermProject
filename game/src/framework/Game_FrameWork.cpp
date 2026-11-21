#include "FrameWork.h"
#include "gameplay/player/player_motion.h"
#include "gameplay/player/player_view.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace
{
	const float PlayerMoveSpeed = 5.f;
	const double MaxSimulationStep = 0.05;
	const float MouseSensitivity = 0.167f;
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool mouseFocused = false;
	bool firstMouse = true;
	double lastMouseX = 0.0;
	double lastMouseY = 0.0;
	double mouseDeltaX = 0.0;
	double mouseDeltaY = 0.0;
}

int GameFrameWork::Run()
{
	if (!Init())
	{
		Clear();
		return 1;
	}

	double previousTime = glfwGetTime();
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		if (glfwWindowShouldClose(window))
			break;

		double currentTime = glfwGetTime();
		double deltaTime = currentTime - previousTime;
		previousTime = currentTime;
		Update(deltaTime);
		if (glfwWindowShouldClose(window))
			break;
		Draw();
	}
	Clear();
	return 0;
}

bool GameFrameWork::Init()
{
	if (!glfwInit())
		return false;
	glfwReady = true;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(1920, 1080, "AEGIS - OpenGL", nullptr, nullptr);
	if (!window)
		return false;
	glfwMakeContextCurrent(window);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
		return false;
	glfwSwapInterval(1);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.08f, 0.11f, 0.15f, 1.0f);

	camera.Init();
	sceneAttempted = true;
	if (!scene.Init("."))
		return false;
	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, KeyCallBack);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetCursorPosCallback(window, CursorPosCallback);
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetWindowFocusCallback(window, WindowFocusCallback);
	glfwSetWindowIconifyCallback(window, WindowFocusCallback);
	WindowFocusCallback(window, glfwGetWindowAttrib(window, GLFW_FOCUSED));

	std::cout << "WASD: move | mouse: aim | LMB: fire | E: use | G: drop | Shift: normal time\n"
		"Enter: next area | R: checkpoint | Esc/Space: pause | Q: quit\n";
	return true;
}

void GameFrameWork::Update(double deltaTime)
{
	glfwGetFramebufferSize(window, &width, &height);
	drawable = width > 0 && height > 0 && !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
	const bool active = drawable && mouseFocused;
	if (resetRequested)
	{
		scene.Reset();
		resetRequested = false;
	}

	const bool inputActive = active && wasActive && !restartFrameTime;
	actualTravel = 0.f;
	movementTimeScale = 0.05;
	SyncPlayerCamera(inputActive);
	glm::vec3 direction(0.f);
	if (cursorCaptured)
	{
		// 마우스와 WASD는 렌더 프레임마다 한 번만 읽는다.
		camera.Rotate(-static_cast<float>(mouseDeltaY) * MouseSensitivity,
			-static_cast<float>(mouseDeltaX) * MouseSensitivity);
		camera.UpdateView();
		SendPlayerView();
		direction = ReadMovementDirection();
	}
	mouseDeltaX = mouseDeltaY = 0.0;
	scene.SampleInput(window, inputActive, fireClicked);
	fireClicked = false;
	SyncPlayerCamera(inputActive);

	if (scene.QuitRequested())
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}
	// 긴 프레임도 남은 시간까지 처리한다. 각 구간의 실제 이동량으로 전투 배속을 정한다.
	double remaining = deltaTime;
	while (inputActive && remaining > 0.0)
	{
		const double step = std::min(remaining, MaxSimulationStep);
		if (scene.PlayerControlsActive())
			MovePlayer(direction, static_cast<float>(step));
		scene.Update(step, step * scene.PreviewTimeScale());
		remaining -= step;
	}

	SyncPlayerCamera(inputActive);
	wasActive = active;
	restartFrameTime = false;
}

void GameFrameWork::MovePlayer(const glm::vec3& direction, float step)
{
	const glm::vec3 desired = direction * (PlayerMoveSpeed * step);
	const auto walls = scene.PlayerObstacles();
	const auto movement = aegis::combat::MovePlayer(camera.camPos, desired, walls);
	camera.camPos = movement.eye;
	camera.camTarget += movement.displacement;
	camera.UpdateView();
	actualTravel += glm::length(glm::vec2(movement.displacement.x, movement.displacement.z));
	movementTimeScale = aegis::combat::MovementTimeScale(movement.displacement, step, PlayerMoveSpeed);
	scene.SetMovementTimeScale(movementTimeScale);
	SendPlayerView();
}

glm::vec3 GameFrameWork::ReadMovementDirection() const
{
	float forwardInput = 0.f;
	float rightInput = 0.f;
	if (moveForward) forwardInput += 1.f;
	if (moveBackward) forwardInput -= 1.f;
	if (moveLeft) rightInput -= 1.f;
	if (moveRight) rightInput += 1.f;
	return aegis::combat::RequestedPlayerMovement(camera.camTarget - camera.camPos,
		forwardInput, rightInput, 1.f);
}

void GameFrameWork::SendPlayerView()
{
	scene.SetPlayerView({ camera.camPos, camera.camTarget - camera.camPos, camera.camUp, camera.fovy });
}

void GameFrameWork::SyncPlayerCamera(bool inputActive)
{
	const bool playing = scene.PlayerControlsActive();
	if (playing && (!cameraSynced || cameraSpawnRevision != scene.PlayerSpawnRevision() ||
		!scene.HasExternalPlayerView()))
	{
		const aegis::PlayerView view = scene.CurrentPlayerView();
		camera.camPos = view.eye;
		camera.camUp = view.up;
		camera.rotX = glm::degrees(std::asin(std::clamp(view.forward.y, -1.f, 1.f)));
		camera.rotY = glm::degrees(std::atan2(-view.forward.x, -view.forward.z));
		camera.Rotate(0.f, 0.f);
		camera.UpdateView();
		movementTimeScale = 0.05;
		actualTravel = 0.f;
		cameraSpawnRevision = scene.PlayerSpawnRevision();
		cameraSynced = true;
		firstMouse = true;
		mouseDeltaX = mouseDeltaY = 0.0;
	}
	SetCursorCapture(playing && inputActive);
	if (playing && inputActive)
	{
		SendPlayerView();
		scene.SetMovementTimeScale(movementTimeScale);
	}
}

void GameFrameWork::Draw()
{
	if (!drawable)
	{
		glfwWaitEventsTimeout(0.1);
		return;
	}
	glViewport(0, 0, width, height);
	camera.UpdateProjection(width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	const Camera* playerCamera = nullptr;
	if (cameraSynced && scene.HasExternalPlayerView() && !scene.AnalysisActive())
	{
		playerCamera = &camera;
	}
	scene.Draw(width, height, playerCamera);
	glfwSwapBuffers(window);
	UpdateWindowTitle();
}

void GameFrameWork::UpdateWindowTitle()
{
	std::string title = scene.StatusTitle();
	title += " | Player: " + std::to_string(camera.camPos.x) + "," + std::to_string(camera.camPos.y) +
		"," + std::to_string(camera.camPos.z) + " | Look: " + std::to_string(camera.rotX) + "," +
		std::to_string(camera.rotY) + " | Travel: " + std::to_string(actualTravel) +
		" | MoveScale: " + std::to_string(movementTimeScale);
	glfwSetWindowTitle(window, title.c_str());
}

void GameFrameWork::Clear()
{
	// 부분 초기화 실패도 GL 컨텍스트가 살아 있을 때 해제한다.
	if (sceneAttempted)
	{
		scene.Clear();
		sceneAttempted = false;
	}
	if (window)
	{
		glfwDestroyWindow(window);
		window = nullptr;
	}
	if (glfwReady)
	{
		glfwTerminate();
		glfwReady = false;
	}
}

void GameFrameWork::SetCursorCapture(bool captured)
{
	if (cursorCaptured == captured)
		return;
	cursorCaptured = captured;
	firstMouse = true;
	mouseDeltaX = mouseDeltaY = 0.0;
	glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void GameFrameWork::CursorPosCallback(GLFWwindow* window, double x, double y)
{
	const auto& game = *static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
	if (!mouseFocused || !game.cursorCaptured)
		return;
	if (!firstMouse)
	{
		mouseDeltaX += x - lastMouseX;
		mouseDeltaY += y - lastMouseY;
	}
	lastMouseX = x;
	lastMouseY = y;
	firstMouse = false;
}

void GameFrameWork::MouseButtonCallback(GLFWwindow* window, int button, int action, int)
{
	auto& game = *static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && game.cursorCaptured)
		game.fireClicked = true;
}

void GameFrameWork::ScrollCallback(GLFWwindow* window, double, double yOffset)
{
	auto& game = *static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
	if (mouseFocused)
		game.scene.ScrollAnalysis(yOffset);
}

void GameFrameWork::WindowFocusCallback(GLFWwindow* window, int)
{
	auto& game = *static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
	mouseFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) && !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
	game.restartFrameTime = true;
	firstMouse = true;
	mouseDeltaX = mouseDeltaY = 0.0;
	if (!mouseFocused)
	{
		moveForward = moveBackward = moveLeft = moveRight = false;
		game.SetCursorCapture(false);
	}
}

void GameFrameWork::KeyCallBack(GLFWwindow* window, int key, int, int action, int)
{
	if (action != GLFW_PRESS && action != GLFW_RELEASE)
		return;
	auto& game = *static_cast<GameFrameWork*>(glfwGetWindowUserPointer(window));
	if (key == GLFW_KEY_Q && action == GLFW_RELEASE)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	else if (key == GLFW_KEY_W) moveForward = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_S) moveBackward = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_A) moveLeft = (action == GLFW_PRESS);
	else if (key == GLFW_KEY_D) moveRight = (action == GLFW_PRESS);
	if (action != GLFW_PRESS)
		return;
	if (key == GLFW_KEY_ESCAPE)
	{
		if (!game.scene.CloseAnalysis())
		{
			game.scene.TogglePause();
		}
		game.restartFrameTime = true;
	}
	else if (key == GLFW_KEY_SPACE && !game.scene.AnalysisActive())
	{
		game.scene.TogglePause();
		game.restartFrameTime = true;
	}
	else if (key == GLFW_KEY_R)
	{
		game.resetRequested = true;
		game.restartFrameTime = true;
	}
}
