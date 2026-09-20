#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"

namespace primebds::commands {
    static bool cmd_feed(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
    REGISTER_COMMAND(feed, "Refill player hunger using saturation!", cmd_feed,
        info.usages = {"/feed [player: player]"};
        info.permissions = {"primebds.command.feed", "primebds.command.feed.other"};
        info.default_permission = "op";
        info.aliases = {"eat"};);

    static bool cmd_feed(PrimeBDS &plugin, endstone::CommandSender &sender,
                         const std::vector<std::string> &args) {
        if (args.size() > 1) return false;
        auto *self = sender.asPlayer();
        if (args.empty() && !self) {
            sender.sendMessage("Specify a player to feed from the console.");
            return false;
        }
        auto targets = args.empty() ? std::vector<endstone::Actor *>{self}
            : utils::getMatchingActors(plugin, args[0], sender);
        int count = 0;
        for (auto *actor : targets) {
            auto *player = actor->asPlayer();
            if (!player) continue;
            if (player != self && !sender.hasPermission("primebds.command.feed.other")) {
                sender.sendMessage("You do not have permission to feed other players.");
                continue;
            }
            const bool applied = plugin.getServer().dispatchCommand(plugin.getServer().getCommandSender(),
                                                                     utils::feedCommand(player->getName()));
            if (!applied) {
                sender.sendMessage("Could not apply saturation to " + player->getName() + "; check the console error.");
                continue;
            }
            player->sendMessage("Saturation applied to refill your hunger.");
            if (player != self) sender.sendMessage("Saturation applied to " + player->getName() + ".");
            ++count;
        }
        if (targets.empty()) sender.sendMessage("No matching players found.");
        else if (!count) sender.sendMessage("No players were fed; check permissions and the console output.");
        return count > 0;
    }
}
