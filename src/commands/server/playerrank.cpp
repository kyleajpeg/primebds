#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/hierarchy.h"
#include "primebds/utils/rank_tools.h"

namespace primebds::commands {
static bool cmd_playerrank(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
REGISTER_COMMAND(playerrank, "Check your rank or another player's saved rank", cmd_playerrank,
    info.usages = utils::playerRankUsages(); info.permissions = {"primebds.command.playerrank"};);

static bool cmd_playerrank(PrimeBDS &plugin, endstone::CommandSender &sender,
                          const std::vector<std::string> &args) {
    if (args.size() > 1) return false;
    std::optional<db::User> user;
    if (args.empty()) {
        const auto *player = sender.asPlayer();
        if (!player) { sender.sendMessage("Usage: /playerrank <gamertag>"); return true; }
        user = plugin.db->getUserByXuid(player->getXuid());
    } else {
        // Online identity wins over ambiguous historical names. Never accept a partial match.
        for (const auto *player : plugin.getServer().getOnlinePlayers()) {
            if (hierarchy::lower(player->getName()) == hierarchy::lower(args[0])) {
                user = plugin.db->getUserByXuid(player->getXuid());
                break;
            }
        }
        if (!user) user = plugin.db->getUniqueUserByName(args[0]);
    }
    if (!user) {
        sender.sendMessage("Player name is unknown or ambiguous. Use their exact last recorded gamertag.");
        return true;
    }
    sender.sendMessage("§e" + user->name + "§r§7's rank is §e" +
                       hierarchy::rankOf(user->internal_rank).name + "§r");
    return true;
}
} // namespace primebds::commands
