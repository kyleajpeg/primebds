#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"

namespace primebds::commands {
    static bool cmd_speed(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
    REGISTER_COMMAND(speed, "Modifies player flyspeed or walkspeed!", cmd_speed,
        info.usages = utils::speedUsages();
        info.permissions = {"primebds.command.speed"};);

    static bool cmd_speed(PrimeBDS &plugin, endstone::CommandSender &sender,
                          const std::vector<std::string> &args) {
        const auto request = utils::parseSpeed(args);
        if (!request) {
            sender.sendMessage("Usage: /speed <nonnegative value> [player] | /speed <walkspeed|flyspeed> <value> [player] | /speed reset [walkspeed|flyspeed|player] [player]");
            return false;
        }
        auto *self = sender.asPlayer();
        if (request->target.empty() && !self) {
            sender.sendMessage("Specify a player when using this command from the console.");
            return false;
        }
        auto targets = request->target.empty() ? std::vector<endstone::Actor *>{self}
            : utils::getMatchingActors(plugin, request->target, sender);
        int count = 0;
        for (auto *actor : targets) {
            auto *player = actor->asPlayer();
            if (!player) continue;
            using Mode = utils::SpeedRequest::Mode;
            auto mode = request->mode;
            if (mode == Mode::Automatic) mode = player->isFlying() ? Mode::Fly : Mode::Walk;
            if (!request->query && (mode == Mode::Walk || mode == Mode::Both))
                player->setWalkSpeed(request->reset ? 0.1f : request->value);
            if (!request->query && (mode == Mode::Fly || mode == Mode::Both))
                player->setFlySpeed(request->reset ? 0.05f : request->value);
            sender.sendMessage(player->getName() + ": walkspeed=" + std::to_string(player->getWalkSpeed()) +
                               ", flyspeed=" + std::to_string(player->getFlySpeed()));
            ++count;
        }
        if (!count) sender.sendMessage("No matching players found.");
        return count > 0;
    }
}
