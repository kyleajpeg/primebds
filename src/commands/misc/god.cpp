/// @file god.cpp
/// Toggle invulnerability with full health and hunger!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"

namespace primebds::commands {

    static bool cmd_god(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(god, "Toggle invulnerability with full health and hunger!", cmd_god,
                     info.usages = {"/god [player: player] [toggle: bool]"};
                     info.permissions = {"primebds.command.god", "primebds.command.god.other"};
                     info.default_permission = "op";);

    /// Toggle invulnerability with full health and hunger!
    static bool cmd_god(PrimeBDS &plugin, endstone::CommandSender &sender,
                        const std::vector<std::string> &args) {
        if (args.empty()) {
            auto *player = sender.asPlayer();
            if (!player) {
                sender.sendMessage("\u00a7cThis command can only be executed by a player");
                return false;
            }
            if (plugin.isgod.enabled(*player)) {
                plugin.isgod.set(*player, false);
                player->sendMessage("\u00a7cYou are no longer invulnerable");
            } else {
                plugin.isgod.set(*player, true);
                plugin.maintainGodMode(*player);
                player->sendMessage("\u00a7aGod mode enabled: invulnerable, with health and hunger kept full");
            }
            return true;
        }
        if (!sender.hasPermission("primebds.command.god.other")) {
            sender.sendMessage("\u00a7cYou do not have permission to modify others' invulnerability");
            return true;
        }
        auto targets = utils::getMatchingActors(plugin, args[0], sender);
        std::string force = (args.size() > 1) ? args[1] : "";
        int updated = 0;
        for (auto *t : targets) {
            auto *p = t->asPlayer();
            if (!p)
                continue;
            bool enable;
            if (force == "true" || force == "on" || force == "1")
                enable = true;
            else if (force == "false" || force == "off" || force == "0")
                enable = false;
            else
                enable = !plugin.isgod.enabled(*p);

            if (enable) {
                plugin.isgod.set(*p, true);
                plugin.maintainGodMode(*p);
                p->sendMessage("\u00a7aGod mode enabled: invulnerable, with health and hunger kept full");
            } else {
                plugin.isgod.set(*p, false);
                p->sendMessage("\u00a7cYou are no longer invulnerable");
            }
            ++updated;
        }
        if (!updated) {
            sender.sendMessage("No matching players were updated.");
            return false;
        }
        sender.sendMessage("\u00a7eInvulnerability updated for " + std::to_string(updated) + " players");
        return true;
    }

} // namespace primebds::commands
