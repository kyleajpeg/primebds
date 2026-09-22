#include "primebds/utils/hierarchy.h"
#include <charconv>
/// @file rank.cpp
/// Manage server ranks!

#include "primebds/commands/command_registry.h"
#include "primebds/plugin.h"
#include "primebds/utils/rank_tools.h"
#include "primebds/utils/config/config_manager.h"
#include "primebds/utils/permissions/permission_manager.h"

#include <algorithm>

namespace primebds::commands {

    static bool cmd_rank(PrimeBDS &, endstone::CommandSender &,
                        const std::vector<std::string> &);

    REGISTER_COMMAND(rank, "Manage server ranks!", cmd_rank,
                     info.usages = {
                         "/rank (set)<sub: rank_sub> <player: string> <rank: string>",
                         "/rank (create)<sub: rank_sub> <name: string>",
                         "/rank (delete)<sub: rank_sub> <name: string>",
                         "/rank (info)<sub: rank_sub> <name: string>",
                         "/rank (perm)<sub: rank_sub> (add|remove)<perm_action: perm_action> <rank: string> <permission: string> [state: bool]",
                         "/rank (list)<sub: rank_sub> [page: int]",
                         "/rank (inherit)<sub: rank_sub> (add|remove)<rank_action: rank_action> <rank: string> <parent: string>",
                         "/rank (weight)<sub: rank_sub> <rank: string> <weight: int>",
                         "/rank (prefix)<sub: rank_sub> <rank: string> <prefix: message>",
                         "/rank (suffix)<sub: rank_sub> <rank: string> <suffix: message>"};
                     info.permissions = {"primebds.command.rank", "primebds.command.rank.set", "primebds.command.rank.list", "primebds.command.rank.info"};);

    static std::string toLower(const std::string &s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

    /// Find the actual key in the permissions JSON (case-insensitive)
    static std::string findRankKey(const nlohmann::json &perms, const std::string &name) {
        auto name_lower = toLower(name);
        for (auto &[key, val] : perms.items()) {
            if (toLower(key) == name_lower)
                return key;
        }
        return "";
    }

    /// Manage server ranks!
    static bool cmd_rank(PrimeBDS &plugin, endstone::CommandSender &sender,
                         const std::vector<std::string> &args) {
        const bool console = hierarchy::isConsole(plugin, sender);
        const bool full = console || (hierarchy::isAdministrator(plugin, sender) && sender.hasPermission("primebds.command.rank"));
        if (!config::ConfigManager::instance().isCommandEnabled("rank")) {
            sender.sendMessage("The rank command is disabled."); return false;
        }
        const auto action = args.empty() ? std::string{} : hierarchy::lower(args[0]);
        if (args.empty() && !full) {
            for (const auto *subcommand : {"set", "list", "info"}) {
                if (!utils::mayUseRankAction(false, subcommand, [&](const std::string &node) { return sender.hasPermission(node); })) continue;
                const std::string sub = subcommand;
                sender.sendMessage("/rank " + sub + (sub == "set" ? " <player> <rank>" : sub == "info" ? " <rank>" : " [page]"));
            }
            return true;
        }
        if (!utils::mayUseRankAction(full, action, [&](const std::string &node) { return sender.hasPermission(node); })) {
            sender.sendMessage("You do not have permission to use this command"); return true;
        }
        if (args.empty()) {
            sender.sendMessage("\u00a7cUsage: /rank <set|create|delete|info|perm|list|inherit|weight|prefix|suffix> ...");
            return false;
        }

        auto &cfg = config::ConfigManager::instance();
        auto &pm = permissions::PermissionManager::instance();
        std::string sub = toLower(args[0]);
        // Refresh all descendants and overrides immediately after every rank-definition edit.
        auto refresh = [&]() {
            pm.reloadPermissionsJson();
            for (auto *player : plugin.getServer().getOnlinePlayers()) plugin.reloadCustomPerms(*player);
        };


        if (sub == "list") {
            auto perms = cfg.loadPermissions();
            if (perms.empty()) {
                sender.sendMessage("\u00a7cNo ranks exist");
                return true;
            }
            sender.sendMessage("\u00a7a--- Ranks ---");
            for (auto &[name, data] : perms.items()) {
                int weight = data.value("weight", 0);
                std::string prefix = data.value("prefix", "");
                sender.sendMessage("\u00a7e" + name + " \u00a77[weight: " + std::to_string(weight) + "]" +
                                   (prefix.empty() ? "" : " \u00a77prefix: " + prefix));
            }
            return true;
        }

        if (sub == "create" && args.size() >= 2) {
            std::string name = args[1];
            auto perms = cfg.loadPermissions();
            if (!findRankKey(perms, name).empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + name + " \u00a7calready exists");
                return true;
            }
            perms[name] = {{"permissions", nlohmann::json::object()}, {"inherits", nlohmann::json::array()}, {"weight", 0}};
            cfg.savePermissions(perms);
            refresh();
            sender.sendMessage("\u00a7aRank \u00a7e" + name + " \u00a7acreated");
            return true;
        }

        if (sub == "delete" && args.size() >= 2) {
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[1]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[1] + " \u00a7cdoes not exist");
                return true;
            }
            if (toLower(key) == "default" || toLower(key) == "owner" || toLower(key) == "operator") {
                sender.sendMessage("\u00a7cCannot delete a reserved rank");
                return true;
            }
            perms.erase(key);
            cfg.savePermissions(perms);
            refresh();
            sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7adeleted");
            return true;
        }

        if (sub == "set" && args.size() >= 3) {
            std::string player_name = args[1];
            std::string rank_name = args[2];
            auto *target = plugin.getServer().getPlayer(player_name);
            if (target && hierarchy::lower(target->getName()) != hierarchy::lower(player_name)) target = nullptr;
            // Never assign a rank to an ambiguous historical name or partial online match.
            if (!target && plugin.db->query("SELECT xuid FROM users WHERE name = ? COLLATE NOCASE", {player_name}).size() != 1) {
                sender.sendMessage("Player name is unknown or ambiguous. Use their exact last recorded gamertag."); return false;
            }
            auto user = target ? plugin.db->getOnlineUser(target->getXuid()) : plugin.db->getUserByName(player_name);
            if (!user) {
                sender.sendMessage("Player record not found. Use the last recorded gamertag of someone who has joined before.");
                return false;
            }
            player_name = user->name;
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, rank_name);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + rank_name + " \u00a7cdoes not exist");
                return false;
            }
            if (!console) {
                const auto destination = hierarchy::rankOf(key);
                const auto grants = pm.getRankPermissions(key);
                const auto op = grants.find("primebds.minecraft.op");
                const bool grants_op = op != grants.end() && op->second;
                if (!hierarchy::canAssign(hierarchy::playerRank(plugin, sender.getName()),
                        hierarchy::rankOf(user->internal_rank), destination,
                        hierarchy::lower(sender.getName()) == hierarchy::lower(user->name), grants_op)) {
                    sender.sendMessage("Rank assignment denied: target and destination must both be strictly below your rank.");
                    return false;
                }
                // Lower-weight rank definitions must not smuggle unrestricted authority.
                if (!full) {
                    for (const auto *danger : {"primebds.command.rank", "primebds.command.permissions",
                                              "minecraft.command.op", "minecraft.command.deop"}) {
                        const auto value = grants.find(danger);
                        if (value != grants.end() && value->second) {
                            sender.sendMessage("That rank carries protected administrative authority."); return false;
                        }
                    }
                }
            }
            const bool changed = hierarchy::lower(hierarchy::rankOf(user->internal_rank).name) != hierarchy::lower(key);
            if (changed) plugin.db->assignRank(user->xuid, key);
            pm.invalidatePermCache(user->xuid);
            pm.clearPrefixSuffixCache();
            if (target) {
                plugin.permissions_pending.insert(user->xuid);
                if (!plugin.reloadCustomPerms(*target)) {
                    sender.sendMessage("Rank saved; permission synchronization is pending. Reconnect the player.");
                    return false;
                }
            }
            sender.sendMessage("\u00a7e" + player_name + " \u00a7arank set to \u00a7e" + key + (target ? "" : " (offline; gameplay changes apply on reconnect)"));
            return true;
        }

        if (sub == "info" && args.size() >= 2) {
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[1]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[1] + " \u00a7cdoes not exist");
                return true;
            }
            auto &data = perms[key];
            sender.sendMessage("\u00a7a--- Rank: " + key + " ---");
            sender.sendMessage("\u00a77Weight: \u00a7e" + std::to_string(data.value("weight", 0)));
            std::string prefix = data.value("prefix", "");
            std::string suffix = data.value("suffix", "");
            if (!prefix.empty())
                sender.sendMessage("\u00a77Prefix: \u00a7r" + prefix);
            if (!suffix.empty())
                sender.sendMessage("\u00a77Suffix: \u00a7r" + suffix);
            if (data.contains("inherits") && data["inherits"].is_array() && !data["inherits"].empty()) {
                std::string inh;
                for (auto &p : data["inherits"]) {
                    if (!inh.empty()) inh += ", ";
                    inh += p.get<std::string>();
                }
                sender.sendMessage("\u00a77Inherits: \u00a7e" + inh);
            }
            if (data.contains("permissions") && data["permissions"].is_object() && !data["permissions"].empty()) {
                sender.sendMessage("\u00a77Permissions:");
                for (auto &[pname, pval] : data["permissions"].items()) {
                    std::string status = pval.get<bool>() ? "\u00a7atrue" : "\u00a7cfalse";
                    sender.sendMessage("  \u00a7b" + pname + " \u00a77= " + status);
                }
            }
            return true;
        }

        if (sub == "perm" && args.size() >= 4) {
            std::string action = toLower(args[1]);
            std::string rank_name = args[2];
            std::string perm = args[3];
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, rank_name);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + rank_name + " \u00a7cdoes not exist");
                return false;
            }
            if (!perms[key].contains("permissions") || !perms[key]["permissions"].is_object())
                perms[key]["permissions"] = nlohmann::json::object();

            if (action == "add") {
                bool state = true;
                if (args.size() >= 5) {
                    const auto value = toLower(args[4]);
                    if (value != "true" && value != "false") {
                        sender.sendMessage("Permission state must be true or false."); return false;
                    }
                    state = value == "true";
                }
                perms[key]["permissions"][perm] = state;
                cfg.savePermissions(perms);
                refresh();
                sender.sendMessage("\u00a7aPermission \u00a7e" + perm + " \u00a7aadded to rank \u00a7e" + key);
            } else if (action == "remove") {
                perms[key]["permissions"].erase(perm);
                cfg.savePermissions(perms);
                refresh();
                sender.sendMessage("\u00a7aPermission \u00a7e" + perm + " \u00a7aremoved from rank \u00a7e" + key);
            }
            return true;
        }

        if (sub == "inherit") {
            // Usage: /rank inherit <add|remove> <rank> <parent>
            if (args.size() < 4) {
                sender.sendMessage("\u00a7cUsage: /rank inherit <add|remove> <rank> <parent>");
                return false;
            }
            std::string action = toLower(args[1]);
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[2]);
            auto parent_key = findRankKey(perms, args[3]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[2] + " \u00a7cdoes not exist");
                return false;
            }
            if (parent_key.empty()) {
                sender.sendMessage("\u00a7cParent rank \u00a7e" + args[3] + " \u00a7cdoes not exist");
                return false;
            }
            if (!perms[key].contains("inherits") || !perms[key]["inherits"].is_array())
                perms[key]["inherits"] = nlohmann::json::array();

            if (action == "add") {
                bool found = false;
                for (auto &p : perms[key]["inherits"]) {
                    if (toLower(p.get<std::string>()) == toLower(parent_key)) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    sender.sendMessage("\u00a7eRank \u00a7e" + key + " \u00a7ealready inherits from \u00a7e" + parent_key);
                    return true;
                }
                perms[key]["inherits"].push_back(parent_key);
                cfg.savePermissions(perms);
                refresh();
                sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7anow inherits from \u00a7e" + parent_key);
            } else if (action == "remove") {
                nlohmann::json new_inherits = nlohmann::json::array();
                bool removed = false;
                for (auto &p : perms[key]["inherits"]) {
                    if (toLower(p.get<std::string>()) != toLower(parent_key))
                        new_inherits.push_back(p);
                    else
                        removed = true;
                }
                if (!removed) {
                    sender.sendMessage("\u00a7cRank \u00a7e" + key + " \u00a7cdoes not inherit from \u00a7e" + parent_key);
                    return true;
                }
                perms[key]["inherits"] = new_inherits;
                cfg.savePermissions(perms);
                refresh();
                sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7ano longer inherits from \u00a7e" + parent_key);
            } else {
                sender.sendMessage("\u00a7cInvalid action '" + args[1] + "': use add or remove");
                return false;
            }
            return true;
        }

        if (sub == "weight" && args.size() >= 3) {
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[1]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[1] + " \u00a7cdoes not exist");
                return false;
            }
            int weight = 0;
            const auto &value = args[2];
            auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), weight);
            if (error != std::errc{} || end != value.data() + value.size()) {
                sender.sendMessage("Rank weight must be a valid integer."); return false;
            }
            perms[key]["weight"] = weight;
            cfg.savePermissions(perms);
            refresh();
            sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7aweight set to \u00a7e" + std::to_string(weight));
            return true;
        }

        if (sub == "prefix" && args.size() >= 3) {
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[1]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[1] + " \u00a7cdoes not exist");
                return false;
            }
            std::string prefix;
            for (size_t i = 2; i < args.size(); ++i) {
                if (i > 2) prefix += " ";
                prefix += args[i];
            }
            perms[key]["prefix"] = prefix;
            cfg.savePermissions(perms);
            refresh();
            sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7aprefix set to \u00a7r" + prefix);
            return true;
        }

        if (sub == "suffix" && args.size() >= 3) {
            auto perms = cfg.loadPermissions();
            auto key = findRankKey(perms, args[1]);
            if (key.empty()) {
                sender.sendMessage("\u00a7cRank \u00a7e" + args[1] + " \u00a7cdoes not exist");
                return false;
            }
            std::string suffix;
            for (size_t i = 2; i < args.size(); ++i) {
                if (i > 2) suffix += " ";
                suffix += args[i];
            }
            perms[key]["suffix"] = suffix;
            cfg.savePermissions(perms);
            refresh();
            sender.sendMessage("\u00a7aRank \u00a7e" + key + " \u00a7asuffix set to \u00a7r" + suffix);
            return true;
        }

        sender.sendMessage("\u00a7cInvalid subcommand for /rank");
        return false;
    }

} // namespace primebds::commands
/// @file rank.cpp
