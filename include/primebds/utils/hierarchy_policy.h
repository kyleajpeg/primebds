#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <map>
#include <string>
#include <vector>

namespace primebds::hierarchy {
inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}
struct Rank {
    std::string name;
    std::optional<std::int64_t> weight;
};
inline bool privileged(const Rank &rank) {
    return rank.weight.has_value() && (lower(rank.name) == "owner" || lower(rank.name) == "operator");
}
inline bool canAccessWarnings(bool admin, bool own, bool editing, bool self_permission,
                              bool staff_permission, bool lower_target) {
    if (admin) return true;
    if (own) return !editing && (self_permission || staff_permission);
    return staff_permission && lower_target;
}
inline bool outranks(const Rank &actor, const Rank &target) {
    if (privileged(actor)) return true;
    if (!actor.weight || !target.weight || lower(actor.name) == lower(target.name)) return false;
    if (lower(target.name) == "owner") return false;
    if (lower(target.name) == "operator" && lower(actor.name) != "owner") return false;
    return *actor.weight > *target.weight;
}
inline bool canTarget(const Rank &actor, const Rank &target, bool same_player, bool allow_self) {
    if (privileged(actor)) return true;
    if (!actor.weight || !target.weight) return false;
    return same_player ? allow_self : outranks(actor, target);
}
inline bool canObserve(const Rank &viewer, const std::vector<Rank> &participants) {
    if (participants.empty()) return false;
    for (const auto &p : participants) if (!outranks(viewer, p)) return false;
    return true;
}
inline bool canAssign(const Rank &actor, const Rank &current, const Rank &destination,
                      bool same_player, bool grants_op) {
    if (privileged(actor)) return true;
    if (same_player || !outranks(actor, current) || !outranks(actor, destination)) return false;
    if (grants_op && lower(actor.name) != "owner") return false;
    return true;
}
/// Quoting-aware tokenizer shared by interception and tests; malformed quotes fail closed.
inline std::optional<std::vector<std::string>> tokenize(const std::string &input) {
    std::vector<std::string> result;
    std::string token;
    bool quoted = false, escaped = false, started = false;
    for (char c : input) {
        if (escaped) { token += c; escaped = false; started = true; }
        else if (c == '\\' && quoted) escaped = true;
        else if (c == '"') { quoted = !quoted; started = true; }
        else if (std::isspace(static_cast<unsigned char>(c)) && !quoted) {
            if (started) { result.push_back(token); token.clear(); started = false; }
        } else { token += c; started = true; }
    }
    if (quoted || escaped) return std::nullopt;
    if (started) result.push_back(token);
    return result;
}
inline std::string canonicalName(std::string name) {
    name = lower(name);
    if (!name.empty() && name.front() == '/') name.erase(0, 1);
    const auto colon = name.find(':');
    if (colon != std::string::npos) {
        auto ns = name.substr(0, colon);
        if (ns == "minecraft" || ns == "endstone" || ns == "primebds") name.erase(0, colon + 1);
    }
    return name;
}
// Limited native teleport grammar; unreviewed facing/selector forms fail closed.
inline bool coordinate(std::string value) {
    if (value.empty()) return false;
    if (value.front() == '~' || value.front() == '^') {
        value.erase(0, 1);
        if (value.empty()) return true;
    }
    if (value.front() == '+' || value.front() == '-') value.erase(0, 1);
    bool digit = false, dot = false;
    for (unsigned char c : value) {
        if (c >= '0' && c <= '9') digit = true;
        else if (c == '.' && !dot) dot = true;
        else return false;
    }
    return digit;
}
inline std::optional<std::vector<std::string>> teleportTargets(std::vector<std::string> args) {
    if (args.empty()) return std::nullopt;
    if (args.back() == "true" || args.back() == "false") args.pop_back();
    auto name = [](const std::string &s) { return !s.empty() && s.front() != '@'; };
    if (args.size() == 1 && name(args[0])) return std::vector<std::string>{args[0]};
    if (args.size() == 2 && name(args[0]) && name(args[1])) return args;
    std::size_t offset;
    if (args.size() == 3 || args.size() == 5) offset = 0;
    else if ((args.size() == 4 || args.size() == 6) && name(args[0])) offset = 1;
    else return std::nullopt;
    for (std::size_t i = offset; i < args.size(); ++i)
        if (!coordinate(args[i])) return std::nullopt;
    return offset ? std::vector<std::string>{args[0]} : std::vector<std::string>{};
}

enum class CommandPolicy { Deny, Ordinary, Selector, Named, Moderation, Owner, Rank, Permissions, Console };
inline CommandPolicy commandPolicy(const std::string &name) {
    static const std::map<std::string, CommandPolicy> policies = [] {
        std::map<std::string, CommandPolicy> p;
        auto add = [&](CommandPolicy policy, const std::string &names) {
            std::string item;
            for (char c : names + " ") {
                if (c == ' ') { if (!item.empty()) p.emplace(item, policy); item.clear(); }
                else item += c;
            }
        };
        add(CommandPolicy::Ordinary, "warnings afk back blockinfo blockscan bottom broadcast clearchat cords discord entityinfo home iteminfo monitor more msgtoggle nickname ping playtime reply rules socialspy spawn spectate speed staffchat top tip toast voice warp activitylist filterlist altspy modspy");
        add(CommandPolicy::Selector, "gma gmc gms gmsp gmt enchantforce giveforce hat itemlore itemname itemtag repair bossbar popup feed god heal fly send");
        add(CommandPolicy::Named, "activity check alts homeother offlinetp permissionslist note");
        add(CommandPolicy::Moderation, "mute nameban nameunban permban punishments removeban silentmute tempban tempmute unmute unwarn warn staffwarnings");
        add(CommandPolicy::Owner, "motd setrules setback sethomes setspawn warps primebds reloadscripts updatepacks");
        add(CommandPolicy::Console, "alist ipban ipmute globalmute");
        p.emplace("rank", CommandPolicy::Rank);
        p.emplace("permissions", CommandPolicy::Permissions);
        return p;
    }();
    auto it = policies.find(name);
    return it == policies.end() ? CommandPolicy::Deny : it->second;
}
} // namespace primebds::hierarchy
