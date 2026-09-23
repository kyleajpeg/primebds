/// @file globalmute.cpp
/// Toggles global mute for the server!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/hierarchy.h"

namespace primebds::commands {

    static bool cmd_globalmute(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(globalmute, "Toggles global mute for the server!", cmd_globalmute,
                     info.usages = {"/globalmute"};
                     info.permissions = {"primebds.command.globalmute"};
                     info.aliases = {"gmute"};);

    /// Toggles global mute for the server!
    static bool cmd_globalmute(PrimeBDS &plugin, endstone::CommandSender &sender,
                               const std::vector<std::string> &args) {
        if (!utils::mayManageGlobalMute(hierarchy::isConsole(plugin, sender), sender.hasPermission("primebds.command.globalmute"))) {
            sender.sendMessage("You do not have permission to use this command"); return true;
        }
        const auto authority = hierarchy::globalMuteAuthority(plugin);
        auto *player = sender.asPlayer();
        if (authority) {
            plugin.globalmute = {};
            plugin.getServer().broadcastMessage("\u00a7a\u00a7lGlobal mute has been disabled by " + sender.getName());
        } else {
            const bool console = hierarchy::isConsole(plugin, sender);
            const auto rank = console ? hierarchy::Rank{"Owner", 0} : hierarchy::playerRank(plugin, sender.getName());
            if (!console && (!player || !rank.weight)) return false;
            plugin.globalmute = {true, console, player ? player->getXuid() : "", rank};
            plugin.getServer().broadcastMessage("\u00a7c\u00a7lGlobal mute has been enabled by " + sender.getName());
        }
        return true;
    }

} // namespace primebds::commands
