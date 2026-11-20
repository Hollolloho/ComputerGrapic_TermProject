// 키보드·마우스를 읽어서 전투 명령으로 바꾼다.
#include "combat_scene.h"
#include "corridor_layout.generated.h"
#include "terminal_layout.generated.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace aegis
{
// 키를 읽고 사격·회수·분석 명령을 전달한다.
void CombatScene::SampleInput(GLFWwindow* window, bool active)
{
    if (DeathStarted())
    {
        active = false;
    }
    const bool preview = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    forceNormal = active && !analysis.Active() &&
                  (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    if (active && !analysis.Active() && preview && !previewHeld)
    {
        if (movementTimeAvailable)
        {
            manualNormalTime = !manualNormalTime;
        }
        else
        {
            slowPreview = !slowPreview;
        }
    }
    previewHeld = preview;
    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool cover = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    const bool drop = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    const bool interact = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    const bool bossToggle = glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
    const bool droneToggle = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
    const bool turret = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    double x{}, y{};
    glfwGetCursorPos(window, &x, &y);
    if (analysis.Active())
    {
        if (active && interact && !interactHeld)
        {
            analysis.Close();
        }
        mouseHeld = left;
        coverHeld = cover;
        lookHeld = false;
        turretHeld = turret;
        droneToggleHeld = droneToggle;
        bossToggleHeld = bossToggle;
        dropHeld = drop;
        interactHeld = interact;
        lastX = x;
        lastY = y;
        return;
    }
    if (active)
    {
        if (debugControls && bossToggle && !bossToggleHeld)
        {
            SetBossMode(!bossMode);
        }
        if (debugControls && !bossMode && corridorMode && droneToggle && !droneToggleHeld)
        {
            SetDroneMode(!IsDroneMode());
        }
        if (debugControls && !bossMode && !corridorMode && turret && !turretHeld)
        {
            SetTurretMode(!turretMode);
        }
        const bool playing = coreMode ? core.Result() == combat::EncounterResult::Running ||
                                        core.Result() == combat::EncounterResult::Securing
                             : bossMode
                                 ? boss.Result() == combat::EncounterResult::Running ||
                                       boss.Result() == combat::EncounterResult::Securing
                                 : !turretMode || encounter.Result() == combat::EncounterResult::Running ||
                                       encounter.Result() == combat::EncounterResult::Securing;
        const bool canEquip = coreMode ? core.Result() != combat::EncounterResult::Failed
                              : bossMode ? boss.Result() != combat::EncounterResult::Failed
                                       : !turretMode || encounter.Result() != combat::EncounterResult::Failed;
        if (!HasExternalPlayerView() && canEquip && right && lookHeld)
        {
            yaw -= static_cast<float>(x - lastX) * 0.14f;
            pitch = std::clamp(pitch - static_cast<float>(y - lastY) * 0.14f, -65.0f, 65.0f);
            camera.SetPose(CameraRoot(), yaw, pitch);
        }
        if (!coreMode && !HasExternalPlayerView() && playing && cover && !coverHeld)
        {
            if (bossMode)
            {
                ToggleBossCover();
            }
            else if (turretMode)
            {
                encounter.ToggleCover();
            }
            else
            {
                range.ToggleCover();
            }
        }
        if (canEquip && drop && !dropHeld)
        {
            if (coreMode)
            {
                core.DropWeapon(PlayerEye(), PlayerForward());
            }
            else if (bossMode)
            {
                boss.DropWeapon(PlayerEye(), PlayerForward());
            }
            else if (turretMode)
            {
                encounter.DropWeapon(PlayerEye(), PlayerForward());
            }
            else
            {
                range.DropWeapon(PlayerEye(), PlayerForward());
            }
        }
        if (canEquip && interact && !interactHeld)
        {
            Interact(PlayerForward());
        }
        if (playing && left && !mouseHeld)
        {
            Fire(PlayerForward());
        }
    }
    mouseHeld = left;
    coverHeld = cover;
    lookHeld = active && right;
    turretHeld = turret;
    droneToggleHeld = droneToggle;
    bossToggleHeld = bossToggle;
    dropHeld = drop;
    interactHeld = interact;
    lastX = x;
    lastY = y;
}

} // namespace aegis
