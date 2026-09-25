#pragma once
#include "primebds/utils/hierarchy_policy.h"
#include <nlohmann/json.hpp>
#include <limits>

namespace primebds::utils {
// Match the hierarchy's signed-integer weight rules without consulting cached ranks.
// Serialization must use the data currently being saved, including just-edited weights.
inline std::optional<std::int64_t> permissionRankWeight(const nlohmann::json &rank) {
    if (!rank.is_object() || !rank.contains("weight") || !rank["weight"].is_number_integer())
        return std::nullopt;
    const auto &weight = rank["weight"];
    if (weight.is_number_unsigned() && weight.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
    return weight.get<std::int64_t>();
}
inline std::string serializePermissions(const nlohmann::json &permissions) {
    if (!permissions.is_object()) return permissions.dump(4, ' ', false);
    std::vector<hierarchy::Rank> ranks;
    for (const auto &[name, data] : permissions.items())
        ranks.push_back({name, permissionRankWeight(data)});
    std::sort(ranks.begin(), ranks.end(), [](const auto &a, const auto &b) {
        if (a.weight.has_value() != b.weight.has_value()) return a.weight.has_value();
        if (a.weight && a.weight != b.weight) return *a.weight < *b.weight;
        const auto left = hierarchy::lower(a.name), right = hierarchy::lower(b.name);
        return left == right ? a.name < b.name : left < right;
    });
    nlohmann::ordered_json ordered = nlohmann::ordered_json::object();
    for (const auto &rank : ranks) ordered[rank.name] = permissions.at(rank.name);
    // JSON objects nested in each rank retain their alphabetical key ordering.
    // Arrays retain their original order; ensure_ascii=false keeps literal section signs.
    return ordered.dump(4, ' ', false);
}
} // namespace primebds::utils
