/// @file god.cpp
/// Toggles invulnerability!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"

namespace primebds::commands {

    static bool cmd_god(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(god, "Toggles invulnerability!", cmd_god,
                     info.usages = {"/god [player: player] [toggle: bool]"};
                     info.permissions = {"primebds.command.god", "primebds.command.god.other"};
                     info.default_permission = "op";);

    /// Toggles invulnerability!
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
                player->sendMessage("\u00a7aYou are now invulnerable");
            }
            return true;
        }
        if (!sender.hasPermission("primebds.command.god.other")) {
            sender.sendMessage("\u00a7cYou do not have permission to modify others' invulnerability");
            return true;
        }
        auto targets = utils::getMatchingActors(plugin.getServer(), args[0], sender);
        std::string force = (args.size() > 1) ? args[1] : "";
        for (auto *t : targets) {
            auto *p = dynamic_cast<endstone::Player *>(t);
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
                p->sendMessage("\u00a7aYou are now invulnerable");
            } else {
                plugin.isgod.set(*p, false);
                p->sendMessage("\u00a7cYou are no longer invulnerable");
            }
        }
        sender.sendMessage("\u00a7eInvulnerability updated for " + std::to_string(targets.size()) + " players");
        return true;
    }

} // namespace primebds::commands
