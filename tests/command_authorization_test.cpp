#include "primebds/handlers/preprocesses/command_authorization.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

using primebds::handlers::preprocesses::canInterceptPlayerCommand;

static void check(bool condition, const char *description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE); // Remain active in Release builds.
    }
}

int main() {
    std::set<std::string> grants = {"minecraft.command.me", "minecraft.command.tell"};
    auto has = [&grants](std::string_view p) { return grants.contains(std::string(p)); };

    // A member cannot reach privileged side effects even if the server has
    // cheats enabled or a stale client command list exposes the command.
    for (const auto *command : {"op", "deop", "kick", "stop", "teleport", "tp",
                               "allowlist", "whitelist", "transfer", "ban", "ban-ip", "banlist",
                               "pardon", "unban", "pardon-ip", "unban-ip", "permban", "tempban",
                               "mute", "tempmute", "ipban", "unmute", "warn", "say"}) {
        check(!canInterceptPlayerCommand(command, has), "member reached a privileged interceptor");
    }
    for (const auto *command : {"me", "tell", "w", "whisper", "msg"})
        check(canInterceptPlayerCommand(command, has), "ordinary member chat alias was blocked");

    // Both nodes are required because these aliases change a persistent rank.
    grants = {"minecraft.command.op"};
    check(!canInterceptPlayerCommand("op", has), "native op grant bypassed rank authorization");
    grants = {"primebds.command.rank"};
    check(!canInterceptPlayerCommand("op", has), "rank grant bypassed native op authorization");
    grants.insert("minecraft.command.op");
    check(canInterceptPlayerCommand("op", has), "authorized op was blocked");
    check(!canInterceptPlayerCommand("deop", has), "op permission implicitly granted deop");
    grants.insert("minecraft.command.deop");
    check(canInterceptPlayerCommand("deop", has), "authorized deop was blocked");
    grants.erase("primebds.command.rank");
    check(!canInterceptPlayerCommand("deop", has), "deop bypassed rank authorization");

    // Delegating one staff action must not grant unrelated authority.
    grants = {"minecraft.command.kick"};
    check(canInterceptPlayerCommand("kick", has), "kick delegation did not work");
    check(!canInterceptPlayerCommand("stop", has), "kick delegation allowed shutdown");
    check(!canInterceptPlayerCommand("op", has), "kick delegation allowed escalation");
    grants = {"minecraft.command.teleport"};
    check(canInterceptPlayerCommand("tp", has) && canInterceptPlayerCommand("teleport", has),
          "teleport alias does not use the same permission");
    grants = {"minecraft.command.allowlist"};
    check(canInterceptPlayerCommand("allowlist", has) && canInterceptPlayerCommand("whitelist", has),
          "allowlist alias does not use the same permission");
    grants = {"endstone.command.unbanip"};
    check(canInterceptPlayerCommand("pardon-ip", has) && canInterceptPlayerCommand("unban-ip", has),
          "IP unban aliases do not use the same permission");
    check(!canInterceptPlayerCommand("pardon", has), "IP unban grant allowed unrelated player unban");

    grants.clear();
    check(!canInterceptPlayerCommand("op", has) && !canInterceptPlayerCommand("deop", has),
          "revoked permissions were retained");

    // Unknown forms are left to normal dispatch, never intercepted blindly.
    auto all = [](std::string_view) { return true; };
    check(!canInterceptPlayerCommand("minecraft:op", all), "unknown namespace was intercepted");
    check(!canInterceptPlayerCommand("future-admin-command", all), "unknown command failed open");
    check(!canInterceptPlayerCommand("", all), "empty command failed open");
    std::cout << "Command authorization regression tests passed.\n";
}
