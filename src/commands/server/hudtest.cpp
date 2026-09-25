#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/hierarchy.h"

namespace primebds::commands {
    static bool cmd_hudtest(PrimeBDS &plugin, endstone::CommandSender &sender,
                           const std::vector<std::string> &args) {
        // A wildcard/Owner permission must not bypass the actual console check.
        const auto result = plugin.hud_test.apply(hierarchy::isConsole(plugin, sender), args);
        if (result == utils::HudTestResult::Denied) {
            sender.sendMessage("HUD diagnostics can only be controlled from the server console.");
            return true;
        }
        if (result == utils::HudTestResult::Usage) {
            sender.sendMessage("Usage: hudtest <baseline|skip|status> OR hudtest <enable|disable> <component>");
            sender.sendMessage("Components: " + utils::hudSelectionList(utils::HudAllComponents) + "; groups: speeds,flight");
            return false;
        }
        const std::string mode = utils::hudModeName(plugin.hud_test.mode());
        sender.sendMessage("[HUDTest] mode=" + mode +
            "; enabled=" + utils::hudSelectionList(plugin.hud_test.selectedMask()) +
            "; disabled=" + utils::hudSelectionList(plugin.hud_test.selectedMask(), false) +
            "; scope=all subsequent initial join synchronizations; online reconciliation remains normal; restart restores baseline.");
        sender.sendMessage("[HUDTest] A successful initial join reconciliation queues one silent FULL repair after 20 server ticks (nominally 1 second), regardless of later switch changes. Partial selections are fully reconciled by that repair; skip schedules none. Loading packets are observational only.");
        if (result == utils::HudTestResult::Changed && plugin.hud_test.mode() != utils::HudTestMode::Baseline)
            sender.sendMessage("[HUDTest] Disabled join operations may leave revoked gameplay/preferences active. Switching back does not reconcile players already online.");
        return true;
    }

    REGISTER_COMMAND(hudtest, "Control the temporary join-reconciliation experiment from the console", cmd_hudtest,
        info.usages = utils::hudTestUsages();
        info.permissions = {"primebds.command.hudtest"};);
} // namespace primebds::commands
