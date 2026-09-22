#pragma once
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace primebds::utils {
struct TeleportDestination {
    double x, y, z;
    std::string dimension;
    std::optional<float> pitch, yaw;
};
inline bool locationNumber(double value) {
    return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}
inline std::optional<TeleportDestination> savedDestination(const std::string &serialized) {
    try {
        const auto data = nlohmann::json::parse(serialized);
        if (!data.is_object() || !data.contains("dimension") || !data["dimension"].is_string()) return std::nullopt;
        for (const auto *key : {"x", "y", "z"})
            if (!data.contains(key) || !data[key].is_number() || !locationNumber(data[key].get<double>())) return std::nullopt;
        TeleportDestination result{data["x"].get<double>(), data["y"].get<double>(), data["z"].get<double>(),
                                   data["dimension"].get<std::string>(), {}, {}};
        if (result.dimension.empty()) return std::nullopt;
        for (const auto *key : {"pitch", "yaw"}) {
            if (!data.contains(key)) continue;
            if (!data[key].is_number() || !locationNumber(data[key].get<double>())) return std::nullopt;
            (std::string(key) == "pitch" ? result.pitch : result.yaw) = data[key].get<float>();
        }
        return result;
    } catch (const std::exception &) { return std::nullopt; }
}
inline std::optional<TeleportDestination> logoutDestination(const std::string &position, const std::string &dimension) {
    try {
        std::vector<std::string> fields;
        std::size_t begin = 0;
        for (;;) {
            const auto comma = position.find(',', begin);
            fields.push_back(position.substr(begin, comma == std::string::npos ? comma : comma - begin));
            if (comma == std::string::npos) break;
            begin = comma + 1;
        }
        if (fields.size() != 3 && fields.size() != 4) return std::nullopt;
        double numbers[3];
        for (int i = 0; i < 3; ++i) {
            std::size_t consumed = 0;
            numbers[i] = std::stod(fields[i], &consumed);
            if (consumed != fields[i].size() || !locationNumber(numbers[i])) return std::nullopt;
        }
        auto name = !dimension.empty() ? dimension : fields.size() == 4 ? fields[3] : "";
        if (name.empty()) return std::nullopt;
        return TeleportDestination{numbers[0], numbers[1], numbers[2], name, {}, {}};
    } catch (const std::exception &) { return std::nullopt; }
}
// One success/failure contract used by the runtime adapter and regression tests.
template <typename Resolve, typename Move>
bool tryTeleport(const std::optional<TeleportDestination> &destination, Resolve resolve, Move move, std::string &error) {
    error.clear();
    if (!destination) { error = "Saved teleport location is missing or invalid."; return false; }
    try {
        auto *dimension = resolve(destination->dimension);
        if (!dimension) { error = "Saved dimension is unavailable."; return false; }
        if (!move(*dimension, *destination)) { error = "Teleport failed or was cancelled."; return false; }
        return true;
    } catch (const std::exception &) { error = "Teleport failed."; return false; }
}
} // namespace primebds::utils
