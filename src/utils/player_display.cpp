#include "primebds/utils/player_display.h"
#include "primebds/plugin.h"
#include "primebds/utils/permissions/permission_manager.h"

namespace primebds::utils {
void sendRankedPlayerList(PrimeBDS &plugin, endstone::CommandSender &sender) {
    std::vector<std::string> names;
    auto &pm = permissions::PermissionManager::instance();
    for (const auto *player : plugin.getServer().getOnlinePlayers()) {
        const auto user = plugin.db->getUserByXuid(player->getXuid());
        const auto prefix = pm.getPrefix(user ? user->internal_rank : "Default");
        names.push_back(rankedPlayerName(prefix, player->getName()));
    }
    for (const auto &message : onlineListMessages(names, plugin.getServer().getMaxPlayers()))
        sender.sendMessage(message);
}
}
