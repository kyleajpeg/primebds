#pragma once

#include "primebds/utils/external_permissions.h"

#include <nlohmann/json.hpp>

namespace primebds::utils::external {

// Collect declarations, not effective booleans: retaining exact-node provenance
// is what lets inherited exceptions survive a descendant's broader group grant.
// Traversal/fallback match the existing legacy rank resolver.
inline Layer collectRankLayer(const nlohmann::json &ranks, const std::string &rank) {
    if (!ranks.is_object()) return {};
    auto find_key = [&](const std::string &name) -> std::string {
        const auto wanted = normalize(name);
        for (const auto &[key, value] : ranks.items())
            if (normalize(key) == wanted) return key;
        return {};
    };
    auto base = find_key(rank);
    if (base.empty()) base = find_key("Default");
    Layer result;
    std::set<std::string> seen;
    std::function<void(const std::string &)> gather = [&](const std::string &name) {
        const auto key = find_key(name);
        if (key.empty() || !seen.insert(key).second) return;
        const auto &group = ranks.at(key);
        if (!group.is_object()) return;
        if (group.contains("inherits") && group["inherits"].is_array())
            for (const auto &parent : group["inherits"])
                if (parent.is_string()) gather(parent.get<std::string>());
        if (!group.contains("permissions")) return;
        const auto &entries = group["permissions"];
        Values values;
        if (entries.is_object()) {
            for (const auto &[node, value] : entries.items()) values[normalize(node)] = value.get<bool>();
        } else if (entries.is_array()) {
            for (const auto &node : entries)
                if (node.is_string()) values[normalize(node.get<std::string>())] = true;
        }
        mergeLayer(result, layerFromValues(values));
    };
    if (!base.empty()) gather(base);
    return result;
}

} // namespace primebds::utils::external
