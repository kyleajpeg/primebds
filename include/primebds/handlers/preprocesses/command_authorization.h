#pragma once

#include <map>
#include <string_view>

namespace primebds::handlers::preprocesses {

// PlayerCommandEvent runs before normal command authorization. Every command
// intercepted here must be authorized before it produces any side effect.
// Unlisted commands fail closed; they must be left to normal server dispatch.
template <typename HasPermission>
bool canInterceptPlayerCommand(std::string_view command, HasPermission has_permission) {
    static const std::map<std::string_view, std::string_view> permissions = {
        {"op", "minecraft.command.op"}, {"deop", "minecraft.command.deop"},
        {"kick", "minecraft.command.kick"}, {"stop", "minecraft.command.stop"},
        {"teleport", "minecraft.command.teleport"}, {"tp", "minecraft.command.teleport"},
        {"allowlist", "minecraft.command.allowlist"}, {"whitelist", "minecraft.command.allowlist"},
        {"transfer", "minecraft.command.transfer"}, {"list", "minecraft.command.list"},
        {"ban", "endstone.command.ban"}, {"ban-ip", "endstone.command.banip"},
        {"banlist", "endstone.command.banlist"},
        {"pardon", "endstone.command.unban"}, {"unban", "endstone.command.unban"},
        {"pardon-ip", "endstone.command.unbanip"}, {"unban-ip", "endstone.command.unbanip"},
        {"permban", "primebds.command.permban"}, {"tempban", "primebds.command.tempban"},
        {"mute", "primebds.command.mute"}, {"tempmute", "primebds.command.tempmute"},
        {"ipban", "primebds.command.ipban"}, {"unmute", "primebds.command.unmute"},
        {"warn", "primebds.command.warn"},
        {"me", "minecraft.command.me"}, {"say", "minecraft.command.say"},
        {"tell", "minecraft.command.tell"}, {"w", "minecraft.command.tell"},
        {"whisper", "minecraft.command.tell"}, {"msg", "minecraft.command.tell"},
    };
    const auto it = permissions.find(command);
    if (it == permissions.end() || !has_permission(it->second))
        return false;

    // These aliases also mutate the persistent PrimeBDS rank, not just native op.
    if ((command == "op" || command == "deop") && !has_permission("primebds.command.rank"))
        return false;
    return true;
}

} // namespace primebds::handlers::preprocesses
