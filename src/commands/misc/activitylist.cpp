#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"

#include <algorithm>
#include <ctime>

namespace primebds::commands {
    static bool cmd_activitylist(PrimeBDS &, endstone::CommandSender &, const std::vector<std::string> &);
    REGISTER_COMMAND(activitylist, "Lists players by activity filter!", cmd_activitylist,
        info.usages = utils::activityListUsages();
        info.permissions = {"primebds.command.activitylist"};);

    static bool cmd_activitylist(PrimeBDS &plugin, endstone::CommandSender &sender,
                                 const std::vector<std::string> &args) {
        const auto request = utils::parseActivityList(args);
        if (!request) {
            sender.sendMessage("Usage: /activitylist [highest|lowest|recent] [positive page], or /activitylist <page> [filter]");
            return false;
        }
        const auto now = static_cast<int64_t>(std::time(nullptr));
        const auto rows = plugin.sldb->getActivitySummary(request->filter, now);
        if (rows.empty()) { sender.sendMessage("No recorded player sessions yet."); return true; }
        constexpr std::size_t page_size = 10;
        const auto pages = (rows.size() + page_size - 1) / page_size;
        if (static_cast<std::size_t>(request->page) > pages) {
            sender.sendMessage("Page out of range. Available pages: 1-" + std::to_string(pages));
            return false;
        }
        sender.sendMessage("Activity List (" + request->filter + ") - Page " + std::to_string(request->page) +
                           "/" + std::to_string(pages));
        const auto first = (static_cast<std::size_t>(request->page) - 1) * page_size;
        for (auto i = first; i < std::min(first + page_size, rows.size()); ++i) {
            const auto &row = rows[i];
            const auto seconds = std::stoll(row.at("total"));
            std::string line = std::to_string(i + 1) + ". " + row.at("name") + ": " +
                std::to_string(seconds / 3600) + "h " + std::to_string((seconds % 3600) / 60) +
                "m " + std::to_string(seconds % 60) + "s";
            if (request->filter == "recent") line += " (last joined " +
                std::to_string(std::max<int64_t>(0, now - std::stoll(row.at("last_join")))) + "s ago)";
            sender.sendMessage(line);
        }
        return true;
    }
}
