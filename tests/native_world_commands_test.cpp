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
        {"weather", "minecraft.command.weather"}};
    for (const auto &[command, node] : commands) {
        check(authorizeDelegatedWorldCommand(command, {}, has) == Decision::MissingPermission,
              "absent grant must deny " + command);
        grants.insert(node);
        for (const auto &[other, other_node] : commands) {
            const auto expected = other == command ? Decision::Allowed : Decision::MissingPermission;
            for (const auto &prefix : {"", "/", "minecraft:", "/minecraft:", "endstone:", "primebds:"})
                check(authorizeDelegatedWorldCommand(prefix + other, {}, has) == expected,
                      "each normalized command must require its own grant: " + other);
        }
        grants.erase(node);
        check(authorizeDelegatedWorldCommand(command, {}, has) == Decision::MissingPermission,
              "revoked grant must deny " + command);
    }
    check(delegatedWorldPermission("/MINECRAFT:SUMMON") == "minecraft.command.summon",
          "command normalization remains case insensitive");
    auto all = [](std::string_view) { return true; };
    for (const auto *name : {"execute", "function", "schedule", "gamerule", "time", "fill", "toggledownfall",
                             "xp", "spawnpoint", "kill", "damage", "effect", "tp", "teleport",
                             "other:summon", "other:weather", "weather:query", "future-command", ""})
        check(authorizeDelegatedWorldCommand(name, {}, all) == Decision::Unhandled,
              std::string("must leave existing policy intact: ") + name);

    // Run the same tokenizer used by live command interception, including quoted names.
    auto decision = [&](const std::string &input) {
        auto tokens = tokenize(input);
        check(tokens && !tokens->empty(), "test command must tokenize");
        return authorizeDelegatedWorldCommand(tokens->front(), {tokens->begin()+1, tokens->end()}, all);
    };
    for (const auto *input : {
        "summon", // Native engine supplies missing-argument and invalid-entity feedback.
        "summon minecraft:pig", "summon invalid_entity",
        "summon pig ~ ~ ~", "summon pig 10 -20.5 +3", "summon pig ^ ^ ^2",
        "summon pig ~1 ~-2 ~.5 90 -30", "summon pig ~ ~ ~ ~ ~",
        "summon pig ~ ~ ~ 90 0 minecraft:entity_born Baby",
        "summon pig ~ ~ ~ 90 0 facing Baby", // Event named facing is not a target keyword.
        "summon pig ~ ~ ~ 90 0 minecraft:entity_born facing",
        "summon pig ~ ~ ~ 90 0 facing \"facing @p\"",
        "summon pig \"A pig facing Owner\" ~ ~ ~",
        "summon pig \"facing\"", "summon pig facing", "summon pig \"facing\" 1 2 3",
        "summon pig facing 0 64 -10", "summon pig facing ~ ~ ~",
        "summon pig ~ ~ ~ facing 1 2 3", "summon pig 1 2 3 facing ^ ^ ^1",
        "summon pig ~ ~ ~ facing ~1 ~-2 ~.5 minecraft:entity_born \"My pet\"",
        "summon pig ~ ~ ~ facing ~ ~ ~ facing facing",
        "/minecraft:summon pig ~ ~ ~ facing 1 2 3 minecraft:entity_born \"facing Owner\"",
        "locate biome plains", "locate structure village", "locate structure village true",
        "weather query", "weather clear", "weather rain 120", "weather thunder",
        "weather nonsense", "locate", // Authorization must not replace native syntax validation.
    }) check(decision(input) == Decision::Allowed, std::string("ordinary native form blocked: ") + input);

    for (const auto *input : {
        "summon pig facing Owner", "summon pig facing \"Higher Rank\"",
        "summon pig ~ ~ ~ facing Owner", "summon pig ~ ~ ~ facing \"Higher Rank\"",
        "summon pig facing @p", "summon pig ~ ~ ~ facing @a",
        "summon pig ~ ~ ~ facing @s", "summon pig ~ ~ ~ facing @e[type=pig,c=1]",
        "summon pig ~ ~ ~ facing @r minecraft:entity_born Pet",
        "summon pig ~ ~ ~ facing \"@e[name=Some Pet]\"",
        "summon pig ~ ~ ~ FACING Owner", "/minecraft:summon pig ~ ~ ~ facing Owner",
        "summon pig ~ ~ ~ facing", "summon pig facing 1", "summon pig facing 1 2",
        "summon pig facing 1 Owner 3", "summon pig facing 1 2 @p",
        "summon pig facing ^ ~ ^", "summon pig facing 1 2 NaN",
        "summon pig facing 1 2 Infinity", "summon pig facing 1 2 ~-",
        "summon pig facing \"1 2 3\"", "summon pig facing \"\"",
        "summon pig ~ ~ ~ facing minecraft:entity_born Pet", // Ambiguous event/target slot fails closed.
        "summon pig ~ ~ ~ facing 1 2 3 facing Owner extra",
    }) check(decision(input) == Decision::FacingRequiresCoordinates,
             std::string("entity/ambiguous facing form escaped the guard: ") + input);

    auto none = [](std::string_view) { return false; };
    check(authorizeDelegatedWorldCommand("summon", {"pig", "facing", "Owner"}, none) == Decision::MissingPermission,
          "facing guard must not skip the permission check");
    check(!tokenize("summon pig \"unfinished"), "malformed quotes fail closed before authorization");
    std::cout << "Delegated native world command permission and summon parser regressions passed.\n";
}
