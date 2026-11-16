#pragma once

namespace aegis {

class GameFramework {
public:
    int Run(bool combatTest = false, bool smokeTest = false, bool turretTest = false, bool droneTest = true, bool bossTest = false);
};

} // namespace aegis
