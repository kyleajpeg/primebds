/// @file offlinetp.cpp
/// Teleport to where a player last logged out.

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/teleport.h"

#include <cstdlib>

namespace primebds::commands {

    static bool cmd_offlinetp(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(offlinetp, "Teleport to where a player last logged out.", cmd_offlinetp,
                     info.usages = {"/offlinetp <player: string>"};
                     info.permissions = {"primebds.command.offlinetp"};
                     info.default_permission = "op";
                     info.aliases = {"otp"};);

    /// Teleport to where a player last logged out.
    static bool cmd_offlinetp(PrimeBDS &plugin, endstone::CommandSender &sender,
                              const std::vector<std::string> &args) {
        auto *player = sender.asPlayer();
        if (!player) {
            sender.sendMessage("\u00a7cOnly players can use this command.");
            return true;
        }

        if (args.empty()) {
            sender.sendMessage("\u00a7cYou must specify a player to teleport to");
            return false;
        }

        for (auto &a : args)
            if (a.find('@') != std::string::npos) {
                sender.sendMessage("\u00a7cTarget selectors are invalid for this command");
                return false;
            }

        std::string target_name = args[0];
        auto user = plugin.db->getUserByName(target_name);
        if (!user || user->last_logout_pos.empty()) {
            sender.sendMessage("\u00a7cNo logout record found for " + target_name);
            return true;
        }

        if (!utils::teleportLogout(plugin, *player, user->last_logout_pos, user->last_logout_dim)) return true;
        sender.sendMessage("Teleported to \u00a7e" + target_name + "\u00a7r's last logout location");
        return true;
    }

} // namespace primebds::commands
