#pragma once
#include <charconv>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace primebds::utils {
inline std::vector<std::string> warningUsages() {
    return {
        "/warn <player: string> <duration: int> (second|seconds|minute|minutes|hour|hours|day|days|week|weeks|month|months|year|years)<unit: warning_unit> <reason: message>",
        "/warn <player: string> (permanent)<duration: warning_permanent> <reason: message>"};
}
struct WarningRequest { std::string target, reason; std::int64_t expires_at = 0; };
// Duration precedes the free-text reason. Never infer duration from a reason suffix.
inline std::optional<WarningRequest> parseWarning(const std::vector<std::string> &args, std::int64_t now) {
    if (args.size() < 3 || args[0].empty() || now < 0) return std::nullopt;
    WarningRequest result{args[0], {}, 0};
    std::size_t reason_start = 2;
    if (args[1] != "permanent") {
        if (args.size() < 4) return std::nullopt;
        std::int64_t count = 0;
        const auto &number = args[1];
        auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), count);
        if (error != std::errc{} || end != number.data() + number.size() || count <= 0) return std::nullopt;
        auto unit = args[2];
        if (unit.ends_with('s')) unit.pop_back();
        static const std::map<std::string, std::int64_t> units = {
            {"second",1}, {"minute",60}, {"hour",3600}, {"day",86400},
            {"week",604800}, {"month",2592000}, {"year",31536000}};
        const auto found = units.find(unit);
        if (found == units.end() || count > (std::numeric_limits<std::int64_t>::max() - now) / found->second)
            return std::nullopt;
        result.expires_at = now + count * found->second;
        reason_start = 3;
    }
    for (std::size_t i = reason_start; i < args.size(); ++i) {
        if (i > reason_start) result.reason += ' ';
        result.reason += args[i];
    }
    if (result.reason.find_first_not_of(" \t\r\n") == std::string::npos) return std::nullopt;
    return result;
}
} // namespace primebds::utils
