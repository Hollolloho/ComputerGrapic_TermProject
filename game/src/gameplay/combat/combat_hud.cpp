#include "combat_scene.h"

#include <array>
#include <string>

namespace aegis
{
namespace
{

// Minimal built-in debug labels: no font package or runtime asset dependency.
std::array<unsigned char, 7> Glyph(char letter)
{
    switch (letter)
    {
    case 'A':
        return {14, 17, 17, 31, 17, 17, 17};
    case 'B':
        return {30, 17, 17, 30, 17, 17, 30};
    case 'C':
        return {14, 17, 16, 16, 16, 17, 14};
    case 'D':
        return {30, 17, 17, 17, 17, 17, 30};
    case 'E':
        return {31, 16, 16, 30, 16, 16, 31};
    case 'F':
        return {31, 16, 16, 30, 16, 16, 16};
    case 'G':
        return {14, 17, 16, 23, 17, 17, 14};
    case 'H':
        return {17, 17, 17, 31, 17, 17, 17};
    case 'I':
        return {31, 4, 4, 4, 4, 4, 31};
    case 'J':
        return {7, 2, 2, 2, 18, 18, 12};
    case 'K':
        return {17, 18, 20, 24, 20, 18, 17};
    case 'L':
        return {16, 16, 16, 16, 16, 16, 31};
    case 'M':
        return {17, 27, 21, 21, 17, 17, 17};
    case 'N':
        return {17, 25, 21, 19, 17, 17, 17};
    case 'O':
        return {14, 17, 17, 17, 17, 17, 14};
    case 'P':
        return {30, 17, 17, 30, 16, 16, 16};
    case 'Q':
        return {14, 17, 17, 17, 21, 18, 13};
    case 'R':
        return {30, 17, 17, 30, 20, 18, 17};
    case 'S':
        return {15, 16, 16, 14, 1, 1, 30};
    case 'T':
        return {31, 4, 4, 4, 4, 4, 4};
    case 'U':
        return {17, 17, 17, 17, 17, 17, 14};
    case 'V':
        return {17, 17, 17, 17, 17, 10, 4};
    case 'W':
        return {17, 17, 17, 21, 21, 21, 10};
    case 'X':
        return {17, 17, 10, 4, 10, 17, 17};
    case 'Y':
        return {17, 17, 10, 4, 4, 4, 4};
    case 'Z':
        return {31, 1, 2, 4, 8, 16, 31};
    case '0':
        return {14, 17, 19, 21, 25, 17, 14};
    case '1':
        return {4, 12, 4, 4, 4, 4, 14};
    case '2':
        return {14, 17, 1, 2, 4, 8, 31};
    case '3':
        return {30, 1, 1, 14, 1, 1, 30};
    case '4':
        return {2, 6, 10, 18, 31, 2, 2};
    case '5':
        return {31, 16, 16, 30, 1, 1, 30};
    case '6':
        return {14, 16, 16, 30, 17, 17, 14};
    case '7':
        return {31, 1, 2, 4, 8, 8, 8};
    case '8':
        return {14, 17, 17, 14, 17, 17, 14};
    case '9':
        return {14, 17, 17, 15, 1, 1, 14};
    case '/':
        return {1, 2, 2, 4, 8, 8, 16};
    default:
        return {};
    }
}

void Label(const std::string& text, int x, int y, int scale, const glm::vec3& color)
{
    glClearColor(color.r, color.g, color.b, 1);
    for (char letter : text)
    {
        const auto glyph = Glyph(letter);
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if (glyph[row] & (1 << (4 - column)))
                {
                    glScissor(x + column * scale, y + (6 - row) * scale, scale, scale);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
        }
        x += 6 * scale;
    }
}

} // namespace

void CombatScene::DrawHud(int width, int height) const
{
    const int scale = width < 900 ? 2 : 3;
    if (IsCoreMode())
    {
        Label("CORE " + std::to_string(core.CoreHealth()) + "/100", 24, height - 40, scale, {0.2f, 1, 0.8f});
        Label("HP " + std::to_string(core.Health()) + "/100", 24, height - 75, scale, {1, 0.8f, 0.3f});
        const char* magnetic = core.Phase() == combat::MagneticPhase::Warning ? "MAGNETIC WARNING / LEAVE RING"
                               : core.Phase() == combat::MagneticPhase::Pulse ? "MAGNETIC PULSE"
                               : core.CoreAlive() ? "DESTROY THE AI CORE" : "CORE DESTROYED";
        Label(magnetic, 24, height - 110, 2, {1, 0.4f, 0.2f});
        Label("LMB FIRE  E SUPPLY  G DROP  R RETRY  ESC MENU", 24, 24, 2, {0.8f, 0.9f, 1});
        return;
    }
    if (IsBossMode())
    {
        DrawBossHud(width, height);
        return;
    }
    if (AnalysisActive())
    {
        Label("DRONE ANALYSIS", 24, height - 40, scale, {0.3f, 1, 0.9f});
        Label("S07 / SENSOR / ROTORS / WEAPON", 24, height - 80, 2, {0.8f, 0.9f, 1});
        Label("E OR ESC RETURN", 24, 24, 2, {0.3f, 1, 0.8f});
        return;
    }
    if (TerminalTarget())
    {
        Label(CanAnalyze() ? "E ANALYZE DRONE" : "ANALYSIS LOCKED / AREA NOT SAFE", width / 2 - 180,
              height / 2 - 80, 2, CanAnalyze() ? glm::vec3(0.3f, 1, 0.8f) : glm::vec3(1, 0.45f, 0.2f));
    }
    if (corridorMode)
    {
        Label("CORRIDOR COMBAT",
              24, height - 135, 2, {0.3f, 0.9f, 1});
    }
    if (CanAnalyze())
    {
        Label("SECTOR SAFE / ANALYSIS READY", 24, height - 165, 2, {0.2f, 1, 0.6f});
    }
    const auto armed = Inventory().Armed();
    Label(armed ? "CW01C READY / G DROP" : "EMPTY / LOOK LEFT DOWN FOR SUPPLY", 24, height - 105, 2,
          armed ? glm::vec3(0.2f, 1, 0.7f) : glm::vec3(1, 0.7f, 0.2f));
    const auto hint = InteractionHint();
    if (hint != combat::WeaponInteraction::None)
    {
        const std::string text = hint == combat::WeaponInteraction::Supply ? "E RESUPPLY" : "E PICK UP";
        Label(text, (width - static_cast<int>(text.size()) * 12) / 2, height / 2 - 50, 2, {0.3f, 1, 0.8f});
    }
    const auto result = encounter.Result();
    const glm::vec3 hpColor =
        encounter.Health() > 25 ? glm::vec3(0.1f, 0.9f, 0.6f) : glm::vec3(1, 0.2f, 0.1f);
    Label("HP " + std::to_string(encounter.Health()) + "/100", 24, height - 40, scale, hpColor);
    glClearColor(0.15f, 0.2f, 0.26f, 1);
    glScissor(24, height - 72, 240, 16);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(hpColor.r, hpColor.g, hpColor.b, 1);
    glScissor(24, height - 72, 240 * encounter.Health() / 100, 16);
    glClear(GL_COLOR_BUFFER_BIT);
    if (result == combat::EncounterResult::Running || result == combat::EncounterResult::Securing)
    {
        std::string phaseLabel = IsDroneMode() ? "PATROL" : "READY";
        if (encounter.Phase() == combat::TurretPhase::Warning)
        {
            phaseLabel = "WARNING";
        }
        if (encounter.Phase() == combat::TurretPhase::Cooldown)
        {
            phaseLabel = "COOLDOWN";
        }
        if (encounter.Phase() == combat::TurretPhase::Disarmed)
        {
            phaseLabel = "DISARMED";
        }
        if (result == combat::EncounterResult::Securing)
        {
            phaseLabel = "INCOMING SHOTS / STAY ALERT";
        }
        Label(phaseLabel, (width - static_cast<int>(phaseLabel.size()) * 6 * scale) / 2, height - 40, scale,
              {1, 0.45f, 0.2f});
    }
    else if (result == combat::EncounterResult::Cleared)
    {
        Label("SECTOR CLEAR", (width - 12 * 6 * scale) / 2, height - 70, scale, {0.2f, 1, 0.65f});
        Label("R RESTART", (width - 9 * 12) / 2, height - 95, 2, {1, 1, 1});
    }
    else
    {
        glClearColor(0.035f, 0.05f, 0.075f, 1);
        glScissor(width / 2 - 220, height / 2, 440, 145);
        glClear(GL_COLOR_BUFFER_BIT);
        const std::string label = result == combat::EncounterResult::Failed ? "FAILED" : "CLEAR";
        const auto color =
            result == combat::EncounterResult::Failed ? glm::vec3(1, 0.25f, 0.2f) : glm::vec3(0.2f, 1, 0.65f);
        const int bigScale = scale * 2;
        Label(label, (width - static_cast<int>(label.size()) * 6 * bigScale) / 2, height / 2 + 60, bigScale,
              color);
        Label("R RETRY", (width - 7 * 6 * scale) / 2, height / 2 + 20, scale, {1, 1, 1});
    }
    if (encounter.DamageFlash() > 0)
    {
        glClearColor(0.8f, 0.04f, 0.07f, 1);
        glScissor(0, 0, width, 6);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, height - 6, width, 6);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, 0, 6, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(width - 6, 0, 6, height);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    Label("LMB FIRE  RMB AIM  E USE  G DROP  R RESET  ESC MENU",
          24, 24, 2, {0.8f, 0.9f, 1});
}

void CombatScene::DrawBossHud(int width, int height) const
{
    const int scale = width < 900 ? 2 : 3;
    Label("HP " + std::to_string(boss.Health()) + "/100", 24, height - 40, scale, {0.2f, 1, 0.7f});
    Label("CORE " + std::to_string(boss.CoreHits()) + "/3", 24, height - 75, scale, {0.3f, 0.9f, 1});
    Label(Inventory().Armed() ? "CW01C READY" : "EMPTY / B COVER THEN E RESUPPLY", 24, height - 105, 2,
          {1, 0.8f, 0.3f});
    const char* phase = boss.Phase() == combat::BossPhase::Exposed   ? "CORE OPEN / FIRE"
                        : boss.Phase() == combat::BossPhase::Warning ? "WARNING / B TAKE COVER"
                        : boss.Phase() == combat::BossPhase::Opening ? "ARMOR OPENING"
                                                                     : "ARMOR CLOSED";
    if (boss.Result() == combat::EncounterResult::Securing)
    {
        phase = "INCOMING SHOTS";
    }
    if (boss.Result() == combat::EncounterResult::Failed)
    {
        phase = "FAILED / R RETRY";
    }
    if (boss.Result() == combat::EncounterResult::Cleared)
    {
        phase = "VICTORY / R RESTART";
    }
    const std::string text = phase;
    Label(text, (width - static_cast<int>(text.size()) * 6 * scale) / 2, height - 40, scale,
          boss.Phase() == combat::BossPhase::Exposed || boss.Result() == combat::EncounterResult::Cleared
              ? glm::vec3(0.2f, 1, 0.8f)
              : glm::vec3(1, 0.4f, 0.2f));
    Label(bossCovered ? "COVER POSITION / B RETURN TO FIRE" : "FIRING POSITION / B TAKE COVER", 24,
          height - 135, 2, {0.7f, 0.9f, 1});
    if (InteractionHint() == combat::WeaponInteraction::Supply)
    {
        Label("E RESUPPLY", width / 2 - 60, height / 2 - 50, 2, {0.3f, 1, 0.8f});
    }
    if (boss.DamageFlash() > 0)
    {
        glClearColor(0.9f, 0.05f, 0.05f, 1);
        glScissor(0, 0, width, 6);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, height - 6, width, 6);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    Label("LMB FIRE  RMB AIM  B COVER  E USE  R RESET  ESC MENU",
          24, 24, 2, {0.8f, 0.9f, 1});
}

std::wstring CombatScene::TimeStatus() const
{
    if (DeathStarted())
    {
        return L"세계 시간 0%";
    }
    const auto current = std::to_wstring(static_cast<int>(time.Scale() * 100 + 0.5));
    return L"세계 시간 " + current + L"%" + (time.ShotRemaining() > 0 ? L" · 사격 진행" : L"");
}

std::string CombatScene::TimeDebug() const
{
    return std::string(" | TimeBase: ") +
           std::to_string(PreviewTimeScale()) +
           " | TimeScale: " + std::to_string(DeathStarted() ? 0.0 : time.Scale()) +
           " | ShotWindow: " + std::to_string(time.ShotRemaining()) + " | DeathFX: " +
           (DeathPlaying()   ? "PLAYING"
            : DeathStarted() ? "DONE"
                             : "OFF") +
           " | Debris: " + std::to_string(destruction.Bursts()) +
           " | TimeMode: AUTO";
}

void CombatScene::PopulateHud(UiPage& page) const
{
    page.timeStatus.clear();
    const auto& inventory = Inventory();
    const auto result = BattleResult();
    page.health = PlayerHealth();
    page.status = L"무기 없음\n재무장 필요";
    if (inventory.Armed())
    {
        page.status = L"CW-01C\n";
        if (DeathStarted() || result != combat::EncounterResult::Running)
            page.status += L"사격 중지";
        else if (time.CanFire())
            page.status += L"사격 가능";
        else
            page.status += L"사격 대기";
    }
    page.footer = inventory.Armed() ? L"G 내려놓기 · Esc 메뉴" : L"Esc 메뉴";
    page.counters.clear();
    page.prompt.clear();
    page.warning.clear();
    if (IsCoreMode())
    {
        page.counters = L"코어 " + std::to_wstring(core.CoreHealth()) +
                       L" / 100\n호위 " +
                       std::to_wstring(core.RemainingEnemies() - (core.CoreAlive() ? 1 : 0)) + L" / 4";
    }
    else if (activeBattle == &encounter)
    {
        page.counters = L"남은 적 " + std::to_wstring(encounter.RemainingEnemies()) + L" / " +
                       std::to_wstring(encounter.EnemyCount());
    }
    else if (IsBossMode())
    {
        page.counters = L"코어 유효타 " + std::to_wstring(boss.CoreHits()) + L" / 3";
    }
    page.danger = page.health <= 25;
    if (result == combat::EncounterResult::Securing)
    {
        page.warning = L"적탄 " + std::to_wstring(activeBattle->EnemyFlights().size()) + L"발 잔류 · 엄폐 유지";
        page.body = L"남은 적탄 " +
                    std::to_wstring(activeBattle->EnemyFlights().size()) +
                    L"발 · 정리 후 진행 가능";
        page.danger = true;
    }
    else if (result == combat::EncounterResult::Cleared)
    {
        if (IsCoreMode())
            page.warning = L"중앙 AI 코어 파괴";
        else if (!combatMapMode && DoorProgress() < 1)
            page.warning = L"출구 문 개방 중";
        else
            page.warning = L"구역 안전 확보";
    }
    else if (IsCoreMode())
    {
        const auto phase = core.Phase();
        page.warning = phase == combat::MagneticPhase::Warning
                           ? L"자기장 예고 · 주황 원 밖으로"
                       : phase == combat::MagneticPhase::Pulse
                           ? L"자기장 방출 · 붉은 원 밖으로"
                           : L"";
        page.danger = page.danger || phase == combat::MagneticPhase::Warning ||
                      phase == combat::MagneticPhase::Pulse;
    }
    else if (IsBossMode())
    {
        switch (boss.Phase())
        {
        case combat::BossPhase::Warning:
            page.warning = bossCovered ? L"보스 조준 · 엄폐 유지" : L"보스 조준 · B 엄폐";
            page.danger = true;
            break;
        case combat::BossPhase::Attack:
            page.warning = L"보스 사격 · 엄폐 유지";
            page.danger = true;
            break;
        case combat::BossPhase::Opening:
            page.warning = L"장갑 개방 중";
            break;
        case combat::BossPhase::Exposed:
            page.warning = bossCovered ? L"코어 노출 · B 사격 위치" : L"코어 노출 · 사격 가능";
            break;
        case combat::BossPhase::Closing:
            page.warning = L"장갑 폐쇄 중 · 피해 차단";
            break;
        default:
            page.warning = L"장갑 닫힘";
            break;
        }
        if (!HasExternalPlayerView())
            page.footer = bossCovered ? L"B 사격 위치 · Esc 메뉴" : L"B 엄폐 · Esc 메뉴";
    }
    else
    {
        switch (encounter.Phase())
        {
        case combat::TurretPhase::Warning:
            page.warning = L"드론 조준 · 사선 이탈";
            page.danger = true;
            break;
        case combat::TurretPhase::Cooldown:
            break;
        case combat::TurretPhase::Disarmed:
            page.warning = L"드론 무장 해제 · 본체 조준";
            break;
        default:
            break;
        }
    }
    if (activeBattle == &encounter && !encounter.Turrets().empty() && result == combat::EncounterResult::Running)
    {
        bool droneWarning = encounter.EnemyAlive() && encounter.Phase() == combat::TurretPhase::Warning;
        for (const auto& droneState : encounter.ExtraDrones())
        {
            droneWarning = droneWarning || droneState.Phase() == combat::DronePhase::Warning;
        }
        page.warning.clear();
        if (droneWarning)
        {
            page.warning = L"드론 조준 · 사선 이탈";
            page.danger = true;
        }
        for (const auto& turret : encounter.Turrets())
        {
            if (turret.Phase() == combat::TurretPhase::Warning)
            {
                page.warning = L"포탑 조준 · 엄폐 또는 사선 이탈";
                page.danger = true;
                break;
            }
        }
    }
    const auto& midboss = encounter.MidbossState();
    if (interlockMode && midboss.Alive() && result == combat::EncounterResult::Running)
    {
        page.counters += L"\nE형 코어 " +
                       std::to_wstring(combat::Midboss::CoreHitLimit - midboss.CoreHits()) + L" / 3";
        if (midboss.Phase() == combat::LaserPhase::Charging)
        {
            page.warning = L"E형 충전 · 엄폐";
            page.danger = true;
        }
        else if (midboss.Phase() == combat::LaserPhase::Firing)
        {
            page.warning = L"E형 레이저 · 사선 이탈";
            page.danger = true;
        }
        else if (midboss.CoreExposed() && page.warning.empty())
        {
            page.warning = L"E형 냉각 · 코어 사격 가능";
            page.danger = false;
        }
    }
    if (IsBossMode() && HasExternalPlayerView() && result == combat::EncounterResult::Running)
    {
        page.warning =
            boss.Phase() == combat::BossPhase::Exposed ? L"코어 노출 · 사격 가능"
            : boss.Phase() == combat::BossPhase::Warning || boss.Phase() == combat::BossPhase::Attack
                ? L"보스 사격 · 엄폐"
                : L"장갑 닫힘";
    }
    if (DeathPlaying())
    {
        page.warning = L"치명상 · 전투 불능";
        page.prompt.clear();
        page.body = L"재시작 메뉴 대기";
        page.danger = true;
        return;
    }
    if (result != combat::EncounterResult::Failed && TerminalTarget())
    {
        page.prompt = CanAnalyze() ? L"E · 드론 분석" : L"적과 적탄 정리 후 분석 가능";
    }
    else if (result != combat::EncounterResult::Failed && !inventory.Armed())
    {
        const auto hint = InteractionHint();
        if (hint == combat::WeaponInteraction::Supply)
            page.prompt = L"E · 무기 보급";
        else if (hint == combat::WeaponInteraction::Pickup)
            page.prompt = L"E · 무기 회수";
    }
}

} // namespace aegis
