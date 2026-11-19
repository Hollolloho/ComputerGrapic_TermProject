// 이전 테스트 호출 호환용. 현재 프로그램의 시작은 main.cpp → FrameWork.h를 따른다.
#pragma once

namespace aegis
{

class GameFramework
{
public:
    int Run(bool combatTest = false, bool smokeTest = false, bool turretTest = false, bool droneTest = true,
            bool bossTest = false, bool campaign = true, bool combatMapTest = false,
            bool playerViewTest = false);
};

} // namespace aegis
