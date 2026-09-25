#include "primebds/utils/hierarchy.h"
#include "primebds/utils/native_world_commands.h"
#include "primebds/plugin.h"
#include "primebds/utils/permissions/permission_manager.h"
#include "primebds/utils/target_selector.h"
#include "primebds/utils/item_slot.h"
#include "primebds/utils/logging.h"
#include "primebds/utils/spy_policy.h"
#include "primebds/utils/command_audit.h"
#include <set>
#include <limits>

namespace primebds::hierarchy {
Rank rankOf(const std::string &name) {
    auto &data = permissions::PermissionManager::instance().PERMISSIONS;
    for (auto &[key, value] : data.items()) {
        if (lower(key) != lower(name)) continue;
        if (!value.is_object() || !value.contains("weight") || !value["weight"].is_number_integer())
            return {key, std::nullopt};
        if (value["weight"].is_number_unsigned() && value["weight"].get<std::uint64_t>() >
                static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return {key, std::nullopt};
        try { return {key, value["weight"].get<std::int64_t>()}; }
        catch (...) { return {key, std::nullopt}; }
    }
    return {name, std::nullopt};
}
Rank playerRank(PrimeBDS &plugin, const std::string &name) {
    auto user = plugin.db->getUserByName(name);
    return user ? rankOf(user->internal_rank) : Rank{"", std::nullopt};
}
bool isConsole(PrimeBDS &plugin, endstone::CommandSender &sender) {
    return &sender == &plugin.getServer().getCommandSender();
}
bool isAdministrator(PrimeBDS &plugin, endstone::CommandSender &sender) {
    if (isConsole(plugin, sender)) return true;
    auto *player = sender.asPlayer();
    if (!player) return false;
    auto rank = playerRank(plugin, player->getName());
    return privileged(rank);
}
std::optional<Rank> globalMuteAuthority(PrimeBDS &plugin) {
    auto &mute = plugin.globalmute;
    if (!mute.enabled) return std::nullopt;
    if (mute.console) return Rank{"Owner", 0};
    const auto user = plugin.db->getUserByXuid(mute.issuer_xuid);
    if (user) {
        auto current = rankOf(user->internal_rank);
        const auto grants = plugin.savedPermissions(user->xuid, user->internal_rank);
        const auto grant = grants.find("primebds.command.globalmute");
        if (current.weight && grant != grants.end() && grant->second) return current;
    }
    mute = {}; // Revoked/deleted authority cannot keep a mute active.
    return std::nullopt;
}
bool isGloballyMuted(PrimeBDS &plugin, endstone::Player &player) {
    const auto current = globalMuteAuthority(plugin);
    if (!current) return false;
    const auto target = playerRank(plugin, player.getName());
    if (utils::globalMuteExempt(player.hasPermission("primebds.command.globalmute"),
            player.hasPermission("primebds.globalmute.exempt"), privileged(target))) return false;
    return plugin.globalmute.console || utils::globalMuteAffects(plugin.globalmute.created, *current, target);
}
bool mayTarget(PrimeBDS &plugin, endstone::CommandSender &sender, const std::string &target, bool allow_self) {
    if (isConsole(plugin, sender)) return true;
    auto *player = sender.asPlayer();
    if (!player) return false;
    return canTarget(playerRank(plugin, player->getName()), playerRank(plugin, target),
                     lower(player->getName()) == lower(target), allow_self);
}
bool requireTarget(PrimeBDS &plugin, endstone::CommandSender &sender, const std::string &target, bool allow_self) {
    if (mayTarget(plugin, sender, target, allow_self)) return true;
    sender.sendMessage("Target denied: you may only affect strictly lower ranks (unknown ranks are protected).");
    return false;
}
bool mayObserve(PrimeBDS &plugin, endstone::Player &viewer, const std::vector<std::string> &participants) {
    std::vector<Rank> ranks;
    for (const auto &name : participants) ranks.push_back(playerRank(plugin, name));
    return canObserve(playerRank(plugin, viewer.getName()), ranks);
}
bool authorizePluginCommand(PrimeBDS &plugin, endstone::CommandSender &sender,
                            const std::string &raw_name, const std::vector<std::string> &args) {
    if (isAdministrator(plugin, sender)) return true;
    if (!sender.asPlayer()) return false;
    auto name = canonicalName(raw_name);
    auto *registration = CommandRegistry::instance().find(name);
    if (registration) name = registration->info.name;
    auto policy = commandPolicy(name);
    if (policy == CommandPolicy::Owner || policy == CommandPolicy::Permissions) {
        if (!isAdministrator(plugin, sender)) {
            sender.sendMessage("You do not have permission to use this command");
            return false;
        }
        if (policy == CommandPolicy::Permissions && !args.empty())
            return requireTarget(plugin, sender, args[0], false);
        return true;
    }
    if (policy == CommandPolicy::Console || policy == CommandPolicy::Deny) {
        sender.sendMessage("You do not have permission to use this command");
        return false;
    }
    if (name == "staffwarnings") return true; // Handler separates self-read from moderation.
    if (policy == CommandPolicy::Shared) return true; // Normal command/subcommand permission gates still apply.
    if (policy == CommandPolicy::Rank) return true; // Checked again inside rank.cpp, including direct callers.
    if (policy == CommandPolicy::Named || policy == CommandPolicy::Moderation) {
        if (args.empty()) return true; // No side effects; handler prints usage.
        if (name == "silentmute") return true; // Filter actual resolved targets, not a second random selection.
        return requireTarget(plugin, sender, args[0], policy == CommandPolicy::Named);
    }
    // Every selector-using handler filters the actual resolved targets in getMatchingActors.
    if (name == "playtime" && !args.empty()) return requireTarget(plugin, sender, args[0]);
    if (name == "ping" && !args.empty() && !args[0].empty() && args[0].front() != '@')
        return requireTarget(plugin, sender, args[0]); // Includes offline fallback.
    return true;
}
bool authorizeNativeCommand(PrimeBDS &plugin, endstone::Player &sender,
                            const std::string &raw_name, const std::vector<std::string> &args) {
    if (isAdministrator(plugin, sender)) return true;
    const auto name = canonicalName(raw_name);
    // Communication is not an administrative action. Spy recipients have their own strict gate.
    static const std::set<std::string> ordinary = {"help", "list", "me", "tell", "w", "whisper", "msg", "say",
        "version", "plugins", "status", "seed", "packstack", "banlist"};
    if (ordinary.contains(name)) return true;
    // Indirect execution and ambiguous/global native commands are not a delegation escape hatch.
    static const std::set<std::string> console_only = {"execute", "function", "schedule", "script", "scriptevent",
        "wsserver", "permission", "allowlist", "whitelist", "ban-ip", "banip", "unban-ip", "pardon-ip",
        "reload", "reloadconfig", "reloadpacketlimitconfig", "changesetting", "gametest"};
    if (console_only.contains(name)) {
        sender.sendMessage("You do not have permission to use this command");
        return false;
    }
    // Delegate only these reviewed world commands, with independent native permissions.
    const auto world_decision = authorizeDelegatedWorldCommand(name,
        [&sender](std::string_view permission) { return sender.hasPermission(std::string(permission)); });
    if (world_decision == DelegatedWorldDecision::MissingPermission) {
        sender.sendMessage("You do not have permission to use this command");
        return false;
    }
    if (world_decision == DelegatedWorldDecision::Allowed) return true; // Keep normal native dispatch.
    // Other world/operational changes remain restricted. Direct player protections
    // do not isolate terrain, summoned entities, weather, PvP or trusted console access.
    static const std::set<std::string> owner_world = {"stop", "save", "difficulty", "gamerule",
        "toggledownfall", "daylock", "fill", "clone", "setblock", "structure", "place",
        "setworldspawn", "mobevent", "tickingarea", "setmaxplayers", "scoreboard"};
    if (owner_world.contains(name)) {
        if (isAdministrator(plugin, sender) && name != "scoreboard") return true;
        sender.sendMessage("You do not have permission to use this command");
        return false;
    }
    std::size_t index = 0;
    bool allow_self = true;
    static const std::set<std::string> first_target = {"kick", "ban", "pardon", "unban", "op", "deop",
        "give", "clear", "kill", "effect", "enchant", "title", "titleraw", "tellraw", "damage", "inputpermission",
        "camera", "hud", "fog", "playanimation", "stopsound", "spawnpoint", "clearspawnpoint", "tag", "transfer"};
    if (name == "gamemode" || name == "xp" || name == "playsound") index = 1;
    else if (name == "teleport" || name == "tp") {
        const auto targets = teleportTargets(args);
        if (!targets) { sender.sendMessage("Use literal names or coordinates; complex teleport forms require the panel."); return false; }
        const auto actor_rank = playerRank(plugin, sender.getName());
        for (const auto &target : *targets) {
            if (!canTeleportTarget(actor_rank, playerRank(plugin, target))) {
                sender.sendMessage("Teleport denied: targets must have an equal or lower rank (unknown ranks are protected).");
                return false;
            }
        }
        return true;
    } else if (!first_target.contains(name)) {
        sender.sendMessage("You do not have permission to use this command");
        return false;
    }
    if (name == "kick" || name == "ban" || name == "pardon" || name == "unban" || name == "op" || name == "deop")
        allow_self = false;
    if (args.size() <= index) return true; // Missing target: native usage/self-only form.
    std::string target = args[index];
    if (target == "@s") target = sender.getName();
    if (target.empty() || target.front() == '@') {
        sender.sendMessage("Native multi-target selectors require the panel; use a player name or an Endstone command.");
        return false;
    }
    return requireTarget(plugin, sender, target, allow_self);
}
void socialSpy(PrimeBDS &plugin, endstone::Player &sender, const std::string &target, const std::string &message) {
    for (auto *viewer : plugin.getServer().getOnlinePlayers()) {
        auto state = plugin.db->getOnlineUser(viewer->getXuid());
        if (state && state->enabled_ss && viewer->hasPermission("primebds.command.socialspy") &&
            mayObserve(plugin, *viewer, {sender.getName(), target}))
            viewer->sendMessage("§8[§eSocialSpy§8] §7" + sender.getName() + " -> " + target + ": §f" + message);
    }
}
void moderationLog(PrimeBDS &plugin, endstone::CommandSender &sender,
                   const std::string &target, const std::string &message) {
    utils::discordRelay(message, "mod");
    for (auto *viewer : plugin.getServer().getOnlinePlayers()) {
        auto state = plugin.db->getOnlineUser(viewer->getXuid());
        if (state && state->enabled_ms && viewer->hasPermission("primebds.command.modspy") &&
            mayObserve(plugin, *viewer, {sender.getName(), target})) viewer->sendMessage("[ModSpy action] " + message);
    }
}

void commandSpy(PrimeBDS &plugin, endstone::Player &sender, const std::string &command) {
    const auto tokens = tokenize(command);
    std::optional<std::vector<std::string>> targets;
    if (tokens && !tokens->empty()) {
        auto name = canonicalName(tokens->front());
        if (const auto *reg = CommandRegistry::instance().find(name)) name = reg->info.name;
        const auto actor = plugin.db->getOnlineUser(sender.getXuid());
        targets = spyTargets(name, {tokens->begin()+1, tokens->end()}, actor ? actor->last_messaged : "");
        if (targets) {
            for (auto &target : *targets) {
                if (target == "@s") target = sender.getName();
                if (target.empty() || target.front() == '@') { targets.reset(); break; }
            }
        }
    }
    for (auto *viewer : plugin.getServer().getOnlinePlayers()) {
        if (viewer == &sender) continue;
        const auto state = plugin.db->getOnlineUser(viewer->getXuid());
        if (!state || !state->enabled_ms || !viewer->hasPermission("primebds.command.modspy")) continue;
        bool visible = isAdministrator(plugin, *viewer);
        if (!visible && targets) {
            auto participants = *targets;
            participants.push_back(sender.getName());
            visible = mayObserve(plugin, *viewer, participants);
        }
        if (visible) viewer->sendMessage("[ModSpy attempt] " + utils::quoteAuditText(sender.getName()) + " " + utils::quoteAuditText(command));
    }
}
} // namespace primebds::hierarchy
