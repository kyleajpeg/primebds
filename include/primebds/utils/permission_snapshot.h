#pragma once
#include <map>
#include <string>

namespace primebds::utils {
inline void applyPluginPermissionGroups(std::map<std::string, bool> &permissions) {
    for (auto &[node, value] : permissions) {
        // Native operator status requires its explicit marker. A plugin command
        // group must not acquire native OP as a side effect of normalization.
        if (node == "primebds.minecraft.op") continue;
        const auto prefix = node.substr(0, node.find('.'));
        if (prefix == "minecraft" || prefix == "endstone") continue;
        auto star = permissions.find(prefix + ".command");
        if (star == permissions.end()) star = permissions.find(prefix);
        if (star != permissions.end()) value = star->second;
    }
}
} // namespace primebds::utils
