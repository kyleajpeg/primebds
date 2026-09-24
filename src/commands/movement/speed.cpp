#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/target_selector.h"
#include <iomanip>
#include <sstream>

namespace primebds::commands {
    static bool cmd_speed(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
    REGISTER_COMMAND(speed, "Set walk/fly speed multipliers (1 = normal)!", cmd_speed,
        info.usages = utils::speedUsages();
        info.permissions = {"primebds.command.speed"};);

    static bool cmd_speed(PrimeBDS &plugin, endstone::CommandSender &sender,
                          const std::vector<std::string> &args) {
        const auto request = utils::parseSpeed(args);
        if (!request) {
            sender.sendMessage("Usage: /speed <nonnegative multiplier> [player] | /speed <walkspeed|flyspeed> <multiplier> [player] | /speed reset [walkspeed|flyspeed|player] [player]");
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
            if (!request->query && (mode == Mode::Walk || mode == Mode::Both)) {
                const auto requested = utils::rawSpeed(request->reset ? 1.0f : request->value, false);
                plugin.getLogger().info("{} event=manual-speed.begin setter=setWalkSpeed requested={} current={} health={}",
                    plugin.hudStamp(player), requested, player->getWalkSpeed(), player->getHealth());
                player->setWalkSpeed(requested);
                plugin.getLogger().info("{} event=manual-speed.end setter=setWalkSpeed current={} health={}",
                    plugin.hudStamp(player), player->getWalkSpeed(), player->getHealth());
            }
            if (!request->query && (mode == Mode::Fly || mode == Mode::Both)) {
                const auto requested = utils::rawSpeed(request->reset ? 1.0f : request->value, true);
                plugin.getLogger().info("{} event=manual-speed.begin setter=setFlySpeed requested={} current={} health={}",
                    plugin.hudStamp(player), requested, player->getFlySpeed(), player->getHealth());
                player->setFlySpeed(requested);
                plugin.getLogger().info("{} event=manual-speed.end setter=setFlySpeed current={} health={}",
                    plugin.hudStamp(player), player->getFlySpeed(), player->getHealth());
            }
            std::ostringstream message;
            message << std::setprecision(6) << player->getName() << ": walkspeed="
                    << utils::speedMultiplier(player->getWalkSpeed(), false) << "x, flyspeed="
                    << utils::speedMultiplier(player->getFlySpeed(), true) << "x";
            sender.sendMessage(message.str());
            ++count;
        }
        if (!count) sender.sendMessage("No matching players found.");
        return count > 0;
    }
}
