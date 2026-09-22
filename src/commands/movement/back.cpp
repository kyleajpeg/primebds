/// @file back.cpp
/// Teleport to your last saved location!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/teleport.h"

#include <ctime>
#include <map>

namespace primebds::commands {

    static bool cmd_back(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(back, "Teleport to your last saved location!", cmd_back,
                     info.usages = {"/back"};
                     info.permissions = {"primebds.command.back"};);

    static std::map<std::string, double> back_cooldowns;

    /// Teleport to your last saved location!
    static bool cmd_back(PrimeBDS &plugin, endstone::CommandSender &sender,
                         const std::vector<std::string> &args) {
        auto *player = sender.asPlayer();
        if (!player) {
            sender.sendMessage("\u00a7cOnly players can use this command.");
            return true;
        }

        // Retrieve last saved location from last_warps table
        auto row = plugin.serverdb->queryRow("SELECT location FROM last_warps WHERE xuid = ?",
                                             {player->getXuid()});
        if (!row || row->at("location").empty()) {
            sender.sendMessage("\u00a7cNo previous location found");
            return true;
        }

        bool exempt = player->hasPermission("primebds.exempt.back");

        // Check cooldown
        double now = (double)std::time(nullptr);
        auto it = back_cooldowns.find(player->getXuid());
        if (!exempt && it != back_cooldowns.end()) {
            double diff = now - it->second;
            if (diff < 5.0) {
                sender.sendMessage("\u00a7cYou must wait before using /back again");
                return true;
            }
        }

        if (!utils::teleportSaved(plugin, *player, row->at("location"))) return true;
        player->sendMessage("§aTeleported to your last location");
        back_cooldowns[player->getXuid()] = now;
        return true;
    }

} // namespace primebds::commands
