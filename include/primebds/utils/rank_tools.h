#pragma once
#include "primebds/utils/hierarchy_policy.h"
#include "primebds/utils/admin_commands.h"
#include <map>
#include <set>

namespace primebds::utils {
inline std::vector<std::string> rankSetUsages() { return {"/rankset <player: string> <rank: string>"}; }
inline std::vector<std::string> rankListUsages() { return {"/ranklist [page: int]"}; }
inline std::vector<std::string> playerRankUsages() { return {"/playerrank [gamertag: string]"}; }
inline std::vector<std::string> rankInfoUsages() { return {"/rankinfo <rank: string>"}; }
inline void sortRanks(std::vector<hierarchy::Rank> &ranks) {
    std::sort(ranks.begin(), ranks.end(), [](const auto &a, const auto &b) {
        if (a.weight.has_value() != b.weight.has_value()) return a.weight.has_value();
        if (a.weight && a.weight != b.weight) return *a.weight > *b.weight;
        const auto left = hierarchy::lower(a.name), right = hierarchy::lower(b.name);
        return left == right ? a.name < b.name : left < right;
    });
}
struct RankPage { std::size_t begin, end, pages; };
inline std::optional<RankPage> rankPage(std::size_t count, int page) {
    const auto pages = std::max<std::size_t>(1, count / 10 + (count % 10 != 0));
    if (page < 1 || static_cast<std::size_t>(page) > pages) return std::nullopt;
    const auto begin = (static_cast<std::size_t>(page) - 1) * 10;
    return RankPage{begin, std::min(begin + 10, count), pages};
}
struct GlobalMuteState {
    bool enabled = false;
    bool console = false;
    std::string issuer_xuid;
    hierarchy::Rank created{"", std::nullopt};
};
template <typename Has>
bool mayUseRankAction(bool full, const std::string &action, Has has) {
    if (full) return true;
    return (action == "set" || action == "list" || action == "info") &&
           has("primebds.command.rank." + action);
}
inline std::vector<std::string> filterListUsages() {
    return {"/filterlist",
        "/filterlist (ranks|ops|default|online|offline|muted|banned|ipbanned)<filter: plist_filter> [page: int]",
        "/filterlist (rank)<filter: rank_filter> <rank: string> [page: int]"};
}
struct RankMembers { std::string name; std::vector<std::string> members; };
// Use persistent identity and rank, including offline users and deleted/unknown ranks.
template <typename Users>
std::map<std::string, RankMembers> rankDirectory(const std::vector<std::string> &ranks,
                                               const Users &users) {
    std::map<std::string, RankMembers> result;
    for (const auto &name : ranks) result[hierarchy::lower(name)].name = name;
    std::set<std::string> seen;
    for (const auto &user : users) {
        if (!seen.insert(user.xuid).second) continue;
        auto &entry = result[hierarchy::lower(user.internal_rank)];
        if (entry.name.empty()) entry.name = user.internal_rank;
        entry.members.push_back(user.name);
    }
    for (auto &[key, entry] : result)
        std::sort(entry.members.begin(), entry.members.end());
    return result;
}
inline bool globalMuteAffects(const hierarchy::Rank &created, const hierarchy::Rank &current,
                              const hierarchy::Rank &target) {
    // A promotion cannot broaden an existing mute; a demotion narrows it immediately.
    return hierarchy::outranks(created, target) && hierarchy::outranks(current, target);
}
inline bool mayManageGlobalMute(bool console, bool permission) {
    return console || permission;
}
inline bool globalMuteExempt(bool permission, bool explicit_exemption, bool privileged) {
    return permission || explicit_exemption || privileged;
}
inline std::string markedNickname(std::string nickname) {
    const auto start = nickname.find_first_not_of('~');
    return "~" + (start == std::string::npos ? std::string{} : nickname.substr(start));
}
inline std::string staffChatMessage(const std::string &name, const std::string &message) {
    return "§8[§cStaff§8] §e" + name + "§7: §f" + message;
}
} // namespace primebds::utils
