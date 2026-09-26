#pragma once

#include "primebds/utils/native_command_policy.h"
#include <optional>
#include <string>

namespace primebds::handlers::preprocesses {

enum class PluginCommandOwner { PrimeBDS, External };
struct RegisteredPluginCommand {
    PluginCommandOwner owner;
    std::string name;
    bool registered = true;
    bool enabled = true;
    bool permissions_safe = true;
};
enum class CommandRoute { Existing, External, Collision, Unavailable, UnsafePermissions };
struct CommandRouting {
    CommandRoute route;
    // Empty means retain the legacy parsed label; otherwise this is the actual own command name.
    std::string policy_name;
};

/// Endstone splits the label at a space; free-form arguments belong to its parser.
inline std::string commandLabel(const std::string &command) {
    return command.substr(0, command.find(' '));
}

/// Match Endstone's command lookup normalization without inventing namespace aliases.
inline std::string commandLookupName(std::string label) {
    label = hierarchy::lower(std::move(label));
    if (!label.empty() && label.front() == '/') label.erase(0, 1);
    return label;
}

/// Identify prefixes reserved for native, framework and ChromeVale commands.
inline std::string trustedCommandNamespace(const std::string &label) {
    const auto colon = label.find(':');
    if (colon == std::string::npos) return {};
    const auto prefix = label.substr(0, colon);
    if (prefix == "minecraft" || prefix == "endstone" || prefix == "primebds") return prefix;
    return {};
}

/// Route only verified registrations; permission checks remain in normal Endstone dispatch.
template <typename IsPrimeCommand>
CommandRouting classifyPluginCommand(const std::string &raw_label,
                                     const std::optional<RegisteredPluginCommand> &resolved,
                                     IsPrimeCommand is_prime_command) {
    const auto label = commandLookupName(raw_label);
    if (!resolved) return {CommandRoute::Existing, {}};
    const auto name = hierarchy::lower(resolved->name);
    const auto label_namespace = trustedCommandNamespace(label);
    const auto name_namespace = trustedCommandNamespace(name);
    if (resolved->owner == PluginCommandOwner::PrimeBDS) {
        if ((!label_namespace.empty() && label_namespace != "primebds") ||
            (!name_namespace.empty() && name_namespace != "primebds")) {
            return {CommandRoute::Collision, {}};
        }
        return {CommandRoute::Existing, hierarchy::canonicalName(name)};
    }
    const auto protected_label = [&](const std::string &value) {
        return is_prime_command(value) || hierarchy::isProtectedNativeCommand(value);
    };
    if (!label_namespace.empty() || !name_namespace.empty() ||
        protected_label(label) || protected_label(name)) {
        return {CommandRoute::Collision, {}};
    }
    if (!resolved->registered || !resolved->enabled) return {CommandRoute::Unavailable, {}};
    if (!resolved->permissions_safe) return {CommandRoute::UnsafePermissions, {}};
    return {CommandRoute::External, {}};
}

} // namespace primebds::handlers::preprocesses
