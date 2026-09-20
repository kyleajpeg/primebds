#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace primebds::utils {

class GodModeState {
public:
    template <typename Player> bool enabled(const Player &player) const {
        return players_.contains(player.getUniqueId().str());
    }
    template <typename Player> void set(const Player &player, bool enabled) {
        const auto id = player.getUniqueId().str();
        if (enabled) players_.insert(id);
        else players_.erase(id);
    }
private:
    std::set<std::string> players_;
};

inline std::vector<std::string> speedUsages() {
    return {
        "/speed <value: float>",
        "/speed (flyspeed|walkspeed)<mode: speed_mode> <value: float> [player: player]",
        "/speed (reset)<action: speed_action> [type_or_player: string] [player: player]",
        "/speed (flyspeed|walkspeed)<mode: speed_mode> (reset)<action: speed_action> [player: player]"};
}
inline std::vector<std::string> nicknameUsages() {
    return {"/nickname [name: string] [player: player]"};
}
inline std::vector<std::string> activityListUsages() {
    return {
        "/activitylist [page: int] (highest|lowest|recent)[filter: activity_filter]",
        "/activitylist (highest|lowest|recent)<filter: activity_filter> [page: int]"};
}

struct SpeedRequest {
    enum class Mode { Automatic, Walk, Fly, Both } mode = Mode::Automatic;
    bool reset = false;
    float value = 0;
    std::string target;
};
inline bool isSpeedMode(const std::string &s) { return s == "walkspeed" || s == "flyspeed"; }
inline SpeedRequest::Mode speedMode(const std::string &s) {
    return s == "flyspeed" ? SpeedRequest::Mode::Fly : SpeedRequest::Mode::Walk;
}
inline std::optional<SpeedRequest> parseSpeed(const std::vector<std::string> &args) {
    if (args.empty() || args.size() > 3) return std::nullopt;
    SpeedRequest result;
    if (args[0] == "reset") {
        result.reset = true;
        result.mode = SpeedRequest::Mode::Both;
        if (args.size() >= 2) {
            if (isSpeedMode(args[1])) result.mode = speedMode(args[1]);
            else {
                if (args.size() != 2 || args[1].empty()) return std::nullopt;
                result.target = args[1];
            }
        }
        if (args.size() == 3) result.target = args[2];
        return result;
    }
    std::string value;
    if (args.size() == 1) value = args[0];
    else {
        if (!isSpeedMode(args[0])) return std::nullopt;
        result.mode = speedMode(args[0]);
        if (args.size() == 3) result.target = args[2];
        if (args[1] == "reset") { result.reset = true; return result; }
        value = args[1];
    }
    char *end = nullptr;
    errno = 0;
    result.value = std::strtof(value.c_str(), &end);
    if (value.empty() || end != value.c_str() + value.size() || errno == ERANGE ||
        !std::isfinite(result.value) || result.value < 0) return std::nullopt;
    return result;
}

struct ActivityListRequest { int page = 1; std::string filter = "highest"; };
inline bool isActivityFilter(const std::string &s) { return s == "highest" || s == "lowest" || s == "recent"; }
inline bool parsePage(const std::string &s, int &page) {
    auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), page);
    return error == std::errc{} && end == s.data() + s.size() && page > 0;
}
inline std::optional<ActivityListRequest> parseActivityList(const std::vector<std::string> &args) {
    ActivityListRequest result;
    if (args.size() > 2) return std::nullopt;
    if (args.empty()) return result;
    if (isActivityFilter(args[0])) {
        result.filter = args[0];
        if (args.size() == 2 && !parsePage(args[1], result.page)) return std::nullopt;
    } else {
        if (!parsePage(args[0], result.page)) return std::nullopt;
        if (args.size() == 2) {
            if (!isActivityFilter(args[1])) return std::nullopt;
            result.filter = args[1];
        }
    }
    return result;
}

inline std::string feedCommand(const std::string &name) {
    std::string quoted = "\"";
    for (char c : name) { if (c == '\\' || c == '"') quoted += '\\'; quoted += c; }
    return "effect " + quoted + "\" saturation 1 19 true";
}
inline bool clearsNickname(const std::vector<std::string> &args) {
    return args.empty() || args[0] == "remove" || args[0] == "reset";
}
} // namespace primebds::utils
