#include "primebds/utils/native_world_commands.h"

#include <cstdlib>
#include <iostream>
#include <set>

using namespace primebds::hierarchy;
using Decision = DelegatedWorldDecision;

static void check(bool condition, const std::string &description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE); // Active in Release builds too.
    }
}

int main() {
    std::set<std::string> grants;
    auto has = [&grants](std::string_view permission) { return grants.contains(std::string(permission)); };
    const std::vector<std::pair<std::string, std::string>> commands = {
        {"summon", "minecraft.command.summon"},
        {"locate", "minecraft.command.locate"},
        {"weather", "minecraft.command.weather"},
        {"time", "minecraft.command.time"}};
    for (const auto &[command, node] : commands) {
        check(authorizeDelegatedWorldCommand(command, has) == Decision::MissingPermission,
              "absent grant must deny " + command);
        grants.insert(node);
        for (const auto &[other, other_node] : commands) {
            const auto expected = other == command ? Decision::Allowed : Decision::MissingPermission;
            for (const auto &prefix : {"", "/", "minecraft:", "/minecraft:", "endstone:", "primebds:"})
                check(authorizeDelegatedWorldCommand(prefix + other, has) == expected,
                      "each normalized command must require its own grant: " + other);
        }
        grants.erase(node);
        check(authorizeDelegatedWorldCommand(command, has) == Decision::MissingPermission,
              "revoked grant must deny " + command);
    }
    check(delegatedWorldPermission("/MINECRAFT:SUMMON") == "minecraft.command.summon",
          "command normalization remains case insensitive");
    auto all = [](std::string_view) { return true; };
    for (const auto *name : {"execute", "function", "schedule", "gamerule", "fill", "toggledownfall",
                             "xp", "spawnpoint", "kill", "damage", "effect", "tp", "teleport",
                             "other:summon", "other:weather", "other:time", "weather:query", "future-command", ""})
        check(authorizeDelegatedWorldCommand(name, all) == Decision::Unhandled,
              std::string("must leave existing policy intact: ") + name);

    check(delegatedWorldPermission("/MINECRAFT:TIME") == "minecraft.command.time",
          "time command normalization remains case insensitive");

    // The shared tokenizer still identifies the command. Authorization intentionally
    // does not consume its arguments: native dispatch owns target resolution and syntax.
    // Source wiring checks also ensure these commands are not intercepted or remapped.
    auto decision = [&](const std::string &input) {
        auto tokens = tokenize(input);
        check(tokens && !tokens->empty(), "test command must tokenize");
        return authorizeDelegatedWorldCommand(tokens->front(), has);
    };
    for (const auto &[command, node] : commands) grants.insert(node);
    for (const auto *input : {
        "time set day", "time set 6000", "time add 1000", "time query daytime",
        "time query gametime", "time query day", "/minecraft:time set night",
        "summon pig ~ ~ ~", "summon pig 10 -20.5 +3 90 -30",
        "summon pig ~ ~ ~ 90 0 minecraft:entity_born Baby",
        "summon pig ~ ~ ~ facing 1 2 3", "summon pig ~ ~ ~ facing ~ ~ ~",
        "summon pig ~ ~ ~ facing ^ ^ ^1", "summon pig \"Test Pig\" ~ ~ ~",
        "summon pig ~ ~ ~ facing Admin", "summon pig ~ ~ ~ facing Owner",
        "summon pig facing \"Higher Rank\"", "/minecraft:summon pig ~ ~ ~ facing Owner",
        "summon pig facing @p", "summon pig ~ ~ ~ facing @a", "summon pig ~ ~ ~ facing @s",
        "summon pig ~ ~ ~ facing @e[type=pig,c=1]", "summon pig ~ ~ ~ facing @r",
        "summon pig ~ ~ ~ facing @p minecraft:entity_born \"My Pet\"",
        "locate biome plains", "locate structure village", "locate structure village true",
        "weather query", "weather clear", "weather rain 120", "weather thunder",
        // Permission checks do not pretend to validate native syntax or report success.
        "summon", "summon invalid_entity", "summon pig ~ ~ ~ facing",
        "time", "time set nonsense", "time add not-a-number", "weather nonsense", "locate",
    }) {
        check(decision(input) == Decision::Allowed, std::string("native command blocked: ") + input);
        const auto tokens = tokenize(input);
        const auto node = std::string(delegatedWorldPermission(tokens->front()));
        grants.erase(node);
        check(decision(input) == Decision::MissingPermission,
              std::string("native command bypassed its revoked permission: ") + input);
        grants.insert(node);
    }
    check(!tokenize("summon pig \"unfinished"), "malformed quotes fail closed before authorization");
    std::cout << "Delegated native world command permission and native-dispatch regressions passed.\n";
}
