// 게임 실행 설정. bool 인자를 순서로 넘기지 않고 options.campaign처럼 이름으로 지정한다.
#pragma once

struct GameRunOptions
{
    bool combatTest{};
    bool smokeTest{};
    bool turretTest{};
    bool droneTest{true};
    bool bossTest{};
    bool campaign{true};
    bool combatMapTest{};
    bool playerViewTest{};
};
