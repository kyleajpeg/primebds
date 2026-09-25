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
    return {};
}

// This is a target guard, not a replacement for Bedrock's command parser.
// Facing is a keyword only immediately after the entity type or spawn position.
// After explicit rotations or the facing position, events/names are ordinary text.
inline bool summonUsesCoordinateFacing(const std::vector<std::string> &args) {
    std::size_t facing;
    if (args.size() > 1 && lower(args[1]) == "facing") {
        if (args.size() == 2) return true; // The name-only form: /summon pig "facing".
        facing = 1;
    } else if (args.size() > 4 && lower(args[4]) == "facing") {
        facing = 4;
    } else {
        return true; // No facing-target slot; native syntax/feedback stays authoritative.
    }

    // Quote information has been removed by the shared tokenizer. If an event/name
    // could also be entity-facing syntax, require an unambiguous coordinate form.
    if (args.size() < facing + 4 || args.size() > facing + 6) return false;
    const bool local = args[facing + 1].starts_with('^');
    for (std::size_t i = facing + 1; i <= facing + 3; ++i) {
        if (!coordinate(args[i]) || args[i].starts_with('^') != local) return false;
    }
    return true;
}

enum class DelegatedWorldDecision { Unhandled, Allowed, MissingPermission, FacingRequiresCoordinates };

template <typename HasPermission>
DelegatedWorldDecision authorizeDelegatedWorldCommand(const std::string &name,
        const std::vector<std::string> &args, HasPermission has_permission) {
    const auto permission = delegatedWorldPermission(name);
    if (permission.empty()) return DelegatedWorldDecision::Unhandled;
    if (!has_permission(permission)) return DelegatedWorldDecision::MissingPermission;
    if (permission == "minecraft.command.summon" && !summonUsesCoordinateFacing(args))
        return DelegatedWorldDecision::FacingRequiresCoordinates;
    return DelegatedWorldDecision::Allowed;
}
} // namespace primebds::hierarchy
