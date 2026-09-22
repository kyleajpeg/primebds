#pragma once
#include "primebds/utils/hierarchy_policy.h"
#include "primebds/utils/admin_commands.h"
#include <set>

namespace primebds::hierarchy {
// Unknown, indirect, global or ambiguous commands are visible only to trusted admins.
// This resolves identities for privacy, never executes selectors a second time.
inline std::optional<std::vector<std::string>> spyTargets(
    const std::string &name, const std::vector<std::string> &args, const std::string &reply_to) {
    if (name == "reply") {
        if (reply_to.empty()) return std::nullopt;
        return std::vector<std::string>{reply_to};
    }
    if (name == "tell" || name == "w" || name == "whisper" || name == "msg") {
        if (args.empty()) return std::nullopt;
        return std::vector<std::string>{args[0]};
    }
    if (name == "speed") {
        auto request = utils::parseSpeed(args);
        if (!request) return std::nullopt;
        return request->target.empty() ? std::vector<std::string>{} : std::vector<std::string>{request->target};
    }
    if (name == "rankset") {
        if (args.size() != 2) return std::nullopt;
        return std::vector<std::string>{args[0]};
    }
    if (name == "rank") {
        if (args.size() < 3 || lower(args[0]) != "set") return std::nullopt;
        return std::vector<std::string>{args[1]};
    }
    if (name == "tp" || name == "teleport") return teleportTargets(args);
    if (name == "gamemode" || name == "xp" || name == "playsound")
        return args.size() < 2 ? std::vector<std::string>{} : std::vector<std::string>{args[1]};
    const auto policy = commandPolicy(name);
    static const std::set<std::string> first = {"kick","ban","unban","pardon","op","deop",
        "gma","gmc","gms","gmsp","gmt","feed","heal","fly","send","ping"};
    if (policy == CommandPolicy::Named || policy == CommandPolicy::Moderation ||
        policy == CommandPolicy::Permissions || first.contains(name))
        return args.empty() ? std::vector<std::string>{} : std::vector<std::string>{args[0]};
    static const std::set<std::string> self = {"afk","back","blockinfo","blockscan","bottom","cords",
        "discord","entityinfo","help","list","monitor","msgtoggle","rules","socialspy","spawn",
        "warnings","top","modspy","altspy","version","plugins","status"};
    if (self.contains(name)) return std::vector<std::string>{};
    return std::nullopt;
}
} // namespace primebds::hierarchy
