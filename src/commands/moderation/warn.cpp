#include "primebds/utils/hierarchy.h"
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
                     info.usages = {"/warn <player: player> <reason: string> [duration: int] [unit: string]"};
                     info.permissions = {"primebds.command.warn"};);

    static const std::map<std::string, int64_t> warn_time_units = {
        {"second", 1}, {"minute", 60}, {"hour", 3600}, {"day", 86400}, {"week", 604800}, {"month", 2592000}, {"year", 31536000}};

    /// Warn a player that they are breaking a rule!
    static bool cmd_warn(PrimeBDS &plugin, endstone::CommandSender &sender,
                         const std::vector<std::string> &args) {
        if (args.size() < 2) {
            sender.sendMessage("\u00a7cUsage: /warn <player> <reason> [duration] [unit]");
            return false;
        }

        std::string target_name = args[0];
        auto user = plugin.db->getUserByName(target_name);
        if (!user) {
            sender.sendMessage("\u00a7cPlayer not found");
            return false;
        }

        int64_t duration_seconds = 0; // Permanent unless a duration is supplied.
        size_t reason_end = args.size();

        // Check if last two args are duration + unit
        if (args.size() >= 4) {
            std::string maybe_unit = args[args.size() - 1];
            std::string maybe_num = args[args.size() - 2];
            bool is_num = !maybe_num.empty();
            for (char c : maybe_num)
                if (!std::isdigit(c)) {
                    is_num = false;
                    break;
                }

            if (is_num) {
                auto it = warn_time_units.find(maybe_unit);
                if (it != warn_time_units.end()) {
                    int64_t duration = 0;
                    auto [end, error] = std::from_chars(maybe_num.data(), maybe_num.data() + maybe_num.size(), duration);
                    if (error != std::errc{} || end != maybe_num.data() + maybe_num.size() || duration <= 0 ||
                        duration > (std::numeric_limits<int64_t>::max() - std::time(nullptr)) / it->second) {
                        sender.sendMessage("Warning duration must be a positive, representable integer."); return false;
                    }
                    duration_seconds = duration * it->second;
                    reason_end = args.size() - 2;
                }
            }
        }

        std::string reason;
        for (size_t i = 1; i < reason_end; ++i) {
            if (i > 1)
                reason += " ";
            reason += args[i];
        }
        if (reason.empty()) {
            sender.sendMessage("\u00a7cProvide a reason");
            return false;
        }

        const int64_t expires_at = duration_seconds ? std::time(nullptr) + duration_seconds : 0;
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
