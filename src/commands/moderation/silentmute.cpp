/// @file silentmute.cpp
/// Silently mutes a player (messages appear only to them)!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"
#include "primebds/utils/hierarchy.h"

namespace primebds::commands {

    static bool cmd_silentmute(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(silentmute, "Silently mutes a player (messages appear only to them)!", cmd_silentmute,
                     info.usages = {"/silentmute <player: player>"};
                     info.permissions = {"primebds.command.silentmute"};
                     info.aliases = {"smute"};);

    /// Silently mutes a player (messages appear only to them)!
    static bool cmd_silentmute(PrimeBDS &plugin, endstone::CommandSender &sender,
                               const std::vector<std::string> &args) {
        if (args.empty()) {
            sender.sendMessage("\u00a7cUsage: /silentmute <player>");
            return false;
        }

        auto targets = utils::getMatchingActors(plugin, args[0], sender);
        for (auto *actor : targets) {
            if (auto *player = actor->asPlayer(); player &&
                !hierarchy::requireTarget(plugin, sender, player->getName(), false)) return false;
        }
        for (auto *t : targets) {
            if (auto *p = t->asPlayer()) {
                auto it = plugin.silentmutes.find(p->getXuid());
                if (it != plugin.silentmutes.end()) {
                    plugin.silentmutes.erase(it);
                    sender.sendMessage("\u00a7e" + p->getName() + " \u00a76is no longer silently muted");
                    hierarchy::moderationLog(plugin, sender, p->getName(), sender.getName() + " removed silent mute from " + p->getName());
                } else {
                    plugin.silentmutes.insert(p->getXuid());
                    sender.sendMessage("\u00a7e" + p->getName() + " \u00a76is now silently muted");
                    hierarchy::moderationLog(plugin, sender, p->getName(), sender.getName() + " silently muted " + p->getName());
                }
            }
        }
        return true;
    }

} // namespace primebds::commands
