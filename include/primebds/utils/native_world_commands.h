#pragma once

#include "primebds/utils/hierarchy_policy.h"
#include <string_view>

namespace primebds::hierarchy {
// These commands affect the shared world, rather than a directly targeted player.
inline std::string_view delegatedWorldPermission(const std::string &raw_name) {
    const auto name = canonicalName(raw_name);
    if (name == "summon") return "minecraft.command.summon";
    if (name == "locate") return "minecraft.command.locate";
    if (name == "weather") return "minecraft.command.weather";
    if (name == "time") return "minecraft.command.time";
    return {};
}

enum class DelegatedWorldDecision { Unhandled, Allowed, MissingPermission };

// All arguments, including summon facing targets, are parsed by the native command.
template <typename HasPermission>
DelegatedWorldDecision authorizeDelegatedWorldCommand(const std::string &name, HasPermission has_permission) {
    const auto permission = delegatedWorldPermission(name);
    if (permission.empty()) return DelegatedWorldDecision::Unhandled;
    if (!has_permission(permission)) return DelegatedWorldDecision::MissingPermission;
    return DelegatedWorldDecision::Allowed;
}
} // namespace primebds::hierarchy
