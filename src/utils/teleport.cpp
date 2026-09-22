#include "primebds/utils/teleport.h"
#include "primebds/utils/teleport_location.h"
#include "primebds/plugin.h"

namespace primebds::utils {
static bool teleport(PrimeBDS &plugin, endstone::Player &player, const std::optional<TeleportDestination> &destination) {
    std::string error;
    const auto current = player.getLocation();
    const bool moved = tryTeleport(destination,
        [&](const std::string &name) -> endstone::Dimension * {
            auto *level = plugin.getServer().getLevel();
            return level ? level->getDimension(name) : nullptr;
        },
        [&](endstone::Dimension &dimension, const TeleportDestination &target) {
            return player.teleport(endstone::Location(dimension, target.x, target.y, target.z,
                target.pitch.value_or(current.getPitch()), target.yaw.value_or(current.getYaw())));
        }, error);
    if (!moved) player.sendMessage("§c" + error);
    return moved;
}
bool teleportSaved(PrimeBDS &plugin, endstone::Player &player, const std::string &serialized) {
    return teleport(plugin, player, savedDestination(serialized));
}
bool teleportLogout(PrimeBDS &plugin, endstone::Player &player, const std::string &position, const std::string &dimension) {
    return teleport(plugin, player, logoutDestination(position, dimension));
}
bool teleportHere(PrimeBDS &plugin, endstone::Player &player, double x, double y, double z) {
    if (!locationNumber(x) || !locationNumber(y) || !locationNumber(z)) return teleport(plugin, player, std::nullopt);
    return teleport(plugin, player, TeleportDestination{x, y, z, player.getDimension().getName(), {}, {}});
}
}
