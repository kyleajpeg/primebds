#include "primebds/utils/hierarchy.h"
#include "primebds/utils/warning_command.h"
/// @file warn.cpp
/// Warn a player that they are breaking a rule!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/moderation.h"
#include "primebds/utils/logging.h"

#include <ctime>
#include <cstdlib>
#include <map>
#include <charconv>
#include <limits>

namespace primebds::commands {

    static bool cmd_warn(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(warn, "Warn a player that they are breaking a rule!", cmd_warn,
                     info.usages = utils::warningUsages();
                     info.permissions = {"primebds.command.warn"};);

    /// Warn a player that they are breaking a rule!
    static bool cmd_warn(PrimeBDS &plugin, endstone::CommandSender &sender,
                         const std::vector<std::string> &args) {
        const auto request = utils::parseWarning(args, std::time(nullptr));
        if (!request) {
            sender.sendMessage("Usage: /warn <player> <positive integer> <second(s)|minute(s)|hour(s)|day(s)|week(s)|month(s)|year(s)> <reason> OR /warn <player> permanent <reason>. Invalid durations are rejected.");
            return false;
        }
        const auto &target_name = request->target;
        const auto &reason = request->reason;
        const auto expires_at = request->expires_at;
        auto user = plugin.db->getUserByName(target_name);
        if (!user) { sender.sendMessage("Player not found."); return false; }
        plugin.db->addWarning(user->xuid, user->name, reason, sender.getName(), expires_at);
        if (auto *target = plugin.getServer().getPlayer(user->name)) {
            const std::string notice = "You received a warning from " + sender.getName() + ": " + reason +
                "\nUse /warnings to view your warning history.";
            target->sendMessage(notice);
            endstone::ActionForm form;
            form.setTitle("Warning");
            form.setContent(notice);
            form.addButton("View my warnings");
            form.addButton("Close");
            form.setOnSubmit([&plugin](endstone::Player *p, int selection) {
                if (p && selection == 0) (void)plugin.getServer().dispatchCommand(*p, "warnings");
            });
            target->sendForm(std::move(form));
        }

        std::string expiration_str = (expires_at ? utils::formatTimeRemaining(expires_at) : "Never (permanent)");
        sender.sendMessage("\u00a76Player \u00a7e" + target_name + " \u00a76was warned for \u00a7e\"" +
                           reason + "\" \u00a76which expires \u00a7e" + expiration_str);
        hierarchy::moderationLog(plugin, sender, target_name, "\u00a76Player \u00a7e" + target_name + " \u00a76was warned by \u00a7e" + sender.getName() + " \u00a76for \u00a7e\"" + reason + "\"");
        return true;
    }

} // namespace primebds::commands
