#pragma once

#include "combat_range.h"
#include "player_camera.h"
#include "turret_encounter.h"
#include "weapon_view.h"
#include "drone.h"
#include "analysis_session.h"
#include "boss_view.h"
#include "korean_ui.h"
#include <GL/glew.h>
#include <filesystem>
#include <string>
#include <functional>

struct GLFWwindow;

namespace aegis {

class CombatTestScene {
public:
    bool Init(const std::filesystem::path& projectDirectory);
    void Reset();
    void SampleInput(GLFWwindow* window, bool active);
    void Update(double realDt, double gameDt);
    void Draw(int width, int height,
        const std::function<void(const glm::mat4&,const glm::vec3&)>& drawEnvironment={},bool showHud=true) const;
    void Clear();
    void SetSmokeFrame(int frame);
    bool SmokePassed() const;
    bool SaveFrame(const std::filesystem::path& path, int width, int height) const;
    const combat::CombatRange& Range() const { return range; }
    void SetTurretMode(bool enabled);
    void SetCorridorMode(bool enabled);
    void SetDroneMode(bool enabled);
    void SetBossMode(bool enabled);
    void SetDebugControls(bool enabled) { debugControls=enabled; }
    combat::EncounterResult BattleResult() const { return bossMode ? boss.Result():encounter.Result(); }
    void CampaignSmokeFrame(int frame);
    void PopulateHud(UiPage& page) const;
    int BossCoreHits() const { return boss.CoreHits(); }
    bool IsBossMode() const { return bossMode; }
    bool IsDroneMode() const { return !bossMode && turretMode && encounter.IsDroneMode(); }
    bool AnalysisActive() const { return analysis.Active(); }
    bool CloseAnalysis() { return analysis.Close(); }
    const combat::AnalysisSession& AnalysisState() const { return analysis; }
    bool CanAnalyze() const { return !bossMode && turretMode && encounter.CanAnalyze(); }
    float DoorProgress() const { return corridorMode ? encounter.DoorProgress() : 0; }
    bool IsCorridorMode() const { return corridorMode; }
    bool IsTurretMode() const { return bossMode || turretMode; }
    int SmokeFrameCount() const { return bossMode ? 2400 : IsDroneMode() ? 1320 : turretMode ? 780 : 300; }
    std::string StatusTitle(bool paused) const;

private:
    bool Fire(const glm::vec3& direction);
    glm::vec3 CameraRoot() const;
    void ToggleBossCover();
    void DrawBossHud(int width,int height) const;
    bool InitTerminal(const std::filesystem::path& root);
    bool TerminalTarget() const;
    bool TerminalTarget(const glm::vec3& direction) const;
    void DrawAnalysis(int width,int height) const;
    const combat::WeaponInventory& Inventory() const;
    bool Interact(const glm::vec3& direction);
    combat::WeaponInteraction InteractionHint() const;
    void DrawHud(int width,int height) const;
    void DrawBox(const glm::mat4& viewProjection, const glm::vec3& minimum,
        const glm::vec3& maximum, const glm::vec3& color) const;
    combat::BossEncounter boss;
    BossView bossView;
    bool debugControls{true};
    bool bossMode{},bossCovered{},bossToggleHeld{},smokeBossFailureSeen{};
    combat::AnalysisSession analysis;
    std::vector<DrawMesh> terminalMeshes;
    combat::CombatRange range;
    combat::TurretEncounter encounter;
    PlayerCamera camera;
    WeaponView weapon;
    Drone drone;
    MeshRenderer droneRenderer;
    GLuint program{};
    GLuint vao{};
    GLuint vbo{};
    GLint transformLocation{-1};
    GLint colorLocation{-1};
    bool mouseHeld{};
    bool coverHeld{};
    bool lookHeld{};
    bool turretHeld{};
    bool droneToggleHeld{};
    bool smokePatrolSeen{};
    bool smokeWarningSeen{};
    bool smokeShotSeen{};
    bool smokeSecuringSeen{};
    bool smokeAnalysisSeen{};
    bool smokeAnalysisReturned{};
    bool smokeWorldFrozen{};
    float smokeDoorSnapshot{};
    bool dropHeld{};
    bool interactHeld{};
    bool turretMode{};
    bool corridorMode{};
    bool smokeFailureSeen{};
    double lastX{};
    double lastY{};
    float yaw{};
    float pitch{};
};

} // namespace aegis
