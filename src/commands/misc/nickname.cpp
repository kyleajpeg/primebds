#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"

namespace primebds::commands {
    static bool cmd_nickname(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
    REGISTER_COMMAND(nickname, "Set or clear a display name!", cmd_nickname,
        info.usages = utils::nicknameUsages();
        info.permissions = {"primebds.command.nickname", "primebds.command.nickname.other"};
        info.aliases = {"nick"};);

    static bool cmd_nickname(PrimeBDS &plugin, endstone::CommandSender &sender,
                             const std::vector<std::string> &args) {
        if (args.size() > 2) return false;
        auto *self = sender.asPlayer();
        if (args.size() < 2 && !self) {
            sender.sendMessage("Usage: /nickname <name|remove|reset> <player>");
            return false;
        }
        auto targets = args.size() < 2 ? std::vector<endstone::Actor *>{self}
            : utils::getMatchingActors(plugin.getServer(), args[1], sender);
        int count = 0;
        for (auto *actor : targets) {
            auto *player = dynamic_cast<endstone::Player *>(actor);
            if (!player) continue;
            if (player != self && !sender.hasPermission("primebds.command.nickname.other")) {
                sender.sendMessage("You do not have permission to change other players' nicknames.");
                continue;
            }
            const bool clear = utils::clearsNickname(args);
            player->setNameTag(clear ? player->getName() : args[0]);
            sender.sendMessage(player->getName() + (clear ? ": nickname cleared." : ": nickname set to " + args[0]));
            ++count;
        }
        if (!count && targets.empty()) sender.sendMessage("No matching players found.");
        return count > 0;
    }
}
