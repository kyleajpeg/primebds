#include <algorithm>
#include "primebds/utils/hierarchy.h"
/// @file warnings.cpp
/// List or delete warnings for a player!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/moderation.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace primebds::commands {

    static bool cmd_warnings(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    static bool cmd_my_warnings(PrimeBDS &plugin, endstone::CommandSender &sender,
                                const std::vector<std::string> &args) {
        auto *player = sender.asPlayer();
        if (!player) { sender.sendMessage("Use /staffwarnings <player> [page] from the console."); return false; }
        int page = 1;
        if (args.size() > 1 || (!args.empty() && !utils::parsePage(args[0], page))) {
            sender.sendMessage("Usage: /warnings [page]"); return false;
        }
        std::vector<std::string> own{player->getName()};
        own.insert(own.end(), args.begin(), args.end());
        return cmd_warnings(plugin, sender, own);
    }
    REGISTER_COMMAND(warnings, "Read your own warning history", cmd_my_warnings,
                     info.usages = {"/warnings [page: int]"};
                     info.permissions = {"primebds.command.warnings.self", "primebds.command.warnings"};);
    REGISTER_COMMAND(staffwarnings, "View or manage a player's warnings", cmd_warnings,
                     info.usages = {
                         "/staffwarnings <player: string> [page: int]",
                         "/staffwarnings <player: string> (delete)<action: staffwarnings_delete> <id: int>",
                         "/staffwarnings <player: string> (clear)<action: staffwarnings_clear>"};
                     info.permissions = {"primebds.command.warnings"};);

    /// List or delete warnings for a player!
    static bool cmd_warnings(PrimeBDS &plugin, endstone::CommandSender &sender,
                             const std::vector<std::string> &args) {
        auto *self = sender.asPlayer();
        if (args.empty() && !self) { sender.sendMessage("Usage: staffwarnings <player> [page]"); return false; }
        const std::string target_name = args.empty() ? self->getName() : args[0];
        const bool own = self && hierarchy::lower(self->getName()) == hierarchy::lower(target_name);
        const bool editing = args.size() >= 2 && (args[1] == "delete" || args[1] == "clear");
        const bool admin = hierarchy::isAdministrator(plugin, sender);
        const bool staff = sender.hasPermission("primebds.command.warnings");
        if (!hierarchy::canAccessWarnings(admin, own, editing,
                sender.hasPermission("primebds.command.warnings.self"), staff,
                hierarchy::mayTarget(plugin, sender, target_name, false))) {
            sender.sendMessage("You may only read your own warnings, or moderate strictly lower ranks.");
            return false;
        }
        auto user = plugin.db->getUserByName(target_name);
        if (!user) {
            sender.sendMessage("\u00a7cPlayer not found");
            return false;
        }

        // Check for delete/clear actions first
        if (args.size() >= 2) {
            std::string action = args[1];
            if (action == "delete" && args.size() >= 3) {
                int id = std::atoi(args[2].c_str());
                const auto owned = plugin.db->getWarnings(user->xuid);
                if (std::none_of(owned.begin(), owned.end(), [&](const auto &w) { return w.id == id; })) {
                    sender.sendMessage("Warning ID does not belong to this player."); return false;
                }
                plugin.db->removeWarning(id);
                sender.sendMessage("\u00a76Warning \u00a7eID " + std::to_string(id) + " \u00a76was erased");
                return true;
            }
            if (action == "clear") {
                auto warns = plugin.db->getWarnings(user->xuid);
                for (auto &w : warns)
                    plugin.db->removeWarning(w.id);
                sender.sendMessage("\u00a76All warnings for \u00a7e" + target_name + " \u00a76were erased");
                return true;
            }
        }

        int page = (args.size() >= 2) ? std::max(1, std::atoi(args[1].c_str())) : 1;
        auto warnings = plugin.db->getWarnings(user->xuid);
        if (warnings.empty()) {
            sender.sendMessage("\u00a76No warnings for \u00a7e" + target_name);
            return true;
        }

        int per_page = 5;
        int total_pages = std::max(1, (int)std::ceil((double)warnings.size() / per_page));
        page = std::min(page, total_pages);
        int start = (page - 1) * per_page;
        int end = std::min(start + per_page, (int)warnings.size());

        sender.sendMessage("\u00a76Warnings for \u00a7e" + target_name +
                           " \u00a77(Page " + std::to_string(page) + "/" + std::to_string(total_pages) + "):");
        for (int i = start; i < end; ++i) {
            auto &w = warnings[i];
            std::string expires = w.expires_at < 0 ? "Legacy expiry unknown" :
                w.expires_at == 0 ? "Permanent" : utils::formatTimeRemaining(w.expires_at);
            sender.sendMessage("\u00a78[\u00a77" + std::to_string(w.id) + "\u00a78] \u00a7f\"" +
                               w.warn_reason + "\" \u00a77- \u00a7e" + w.added_by +
                               " \u00a78[\u00a7e" + expires + "\u00a78]");
        }
        return true;
    }

} // namespace primebds::commands
