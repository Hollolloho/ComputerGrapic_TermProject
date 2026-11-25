// 키보드·마우스를 읽어서 전투 명령으로 바꾼다.
#include "combat_scene.h"
#include "combat_map/corridor_layout.generated.h"
#include "combat_map/terminal_layout.generated.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace aegis
{
// 키를 읽고 사격·회수·분석 명령을 전달한다.
void CombatScene::SampleInput(GLFWwindow* window, bool canReceiveInput, bool fireClicked)
{
    if (DeathStarted())
    {
        canReceiveInput = false;
    }
    forceNormal = canReceiveInput && !analysis.Active() &&
                  (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool cover = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool drop = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    const bool interact = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

    double x{}, y{};
    glfwGetCursorPos(window, &x, &y);

    if (analysis.Active())
    {
        const bool leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (canReceiveInput && leftMouseDown && analysisDragHeld)
        {
            analysis.Orbit(static_cast<float>(x - lastX), static_cast<float>(y - lastY));
        }
        if (canReceiveInput && interact && !interactHeld)
        {
            analysis.Close();
        }

        analysisDragHeld = analysis.Active() && canReceiveInput && leftMouseDown;
        coverHeld = cover;
        lookHeld = false;
        dropHeld = drop;
        interactHeld = interact;
        lastX = x;
        lastY = y;
        return;
    }

    analysisDragHeld = false;

    // 분석 화면은 위에서 입력 처리를 끝낸다. 여기부터는 일반 전투 입력이다.
    if (canReceiveInput)
    {
        const auto result = BattleResult();
        const bool playing = result == combat::EncounterResult::Running ||
                             result == combat::EncounterResult::Securing;
        const bool canEquip = result != combat::EncounterResult::Failed;

        if (!HasExternalPlayerView() && canEquip && right && lookHeld)
        {
            rotY -= static_cast<float>(x - lastX) * 0.14f;
            rotX = std::clamp(rotX - static_cast<float>(y - lastY) * 0.14f, -65.0f, 65.0f);
            camera.SetPose(CameraRoot(), rotY, rotX);
        }

        if (!IsCoreMode() && !HasExternalPlayerView() && playing && cover && !coverHeld)
        {
            if (IsBossMode())
            {
                ToggleBossCover();
            }
            else
            {
                encounter.ToggleCover();
            }
        }

        if (canEquip && drop && !dropHeld)
        {
            activeBattle->DropWeapon(PlayerEye(), PlayerForward());
        }

        if (canEquip && interact && !interactHeld)
        {
            Interact(PlayerForward());
        }

        if (fireClicked && !analysis.Active())
        {
            Fire(PlayerForward());
        }

    }
    coverHeld = cover;
    lookHeld = canReceiveInput && right;
    dropHeld = drop;
    interactHeld = interact;
    lastX = x;
    lastY = y;
}

} // namespace aegis
