#pragma once

// Test-only server locator wiring. Permission expansion and all attachment
// classes come unchanged from the fetched Endstone source and public headers.
#include "endstone/plugin/plugin.h"
#include "endstone/plugin/plugin_manager.h"

namespace endstone::core {
class EndstoneServer {
public:
    static EndstoneServer &getInstance() {
        static EndstoneServer instance;
        return instance;
    }
    PluginManager &getPluginManager() { return *plugin_manager; }
    inline static PluginManager *plugin_manager = nullptr;
};
} // namespace endstone::core
