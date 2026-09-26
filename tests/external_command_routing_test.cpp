#include "primebds/handlers/preprocesses/command_routing.h"

#include <cstdlib>
#include <iostream>
#include <set>

using namespace primebds::handlers::preprocesses;

static void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    const std::set<std::string> own_labels = {"rank", "rankset", "rset", "primebds", "mute", "gmc"};
    const auto own = [&](const std::string &name) { return own_labels.contains(name); };
    const auto route = [&](const std::string &label, std::optional<RegisteredPluginCommand> command) {
        return classifyPluginCommand(label, command, own);
    };
    const auto external = [](std::string name) {
        return RegisteredPluginCommand{PluginCommandOwner::External, std::move(name)};
    };

    check(commandLookupName("/MiXeD:Alias") == "mixed:alias", "Lookup retains namespace and folds case");
    check(commandLookupName("//set") == "/set", "Only one leading slash is removed");
    for (const auto *label : {"paint", "/paint", "/BRUSH", "drawing:paint", "other:rank", "//paint"}) {
        check(route(label, external("paint")).route == CommandRoute::External,
              "An actual external registration or alias can continue through Endstone");
    }
    for (const auto *label : {"paint", "missing:paint", "other:rank", "", "unknown-command"}) {
        check(route(label, std::nullopt).route == CommandRoute::Existing,
              "A label without an actual plugin registration must retain native/unknown authorization");
    }
    check(route("/MINECRAFT:Op", std::nullopt).policy_name.empty(),
          "Unresolved native labels retain the legacy parsing and namespace authorization");
    check(route("other:op", std::nullopt).policy_name.empty(),
          "Unregistered namespace is not invented by routing");
    const std::string free_text = "/paint a message with an unmatched \"quote";
    check(commandLabel(free_text) == "/paint" && !primebds::hierarchy::tokenize(free_text) &&
              route(commandLabel(free_text), external("paint")).route == CommandRoute::External,
          "External free-form arguments never pass through the legacy quote parser");

    for (const auto *label : {"rankset", "rset", "/RSET", "primebds:rankset"}) {
        const auto result = route(label, RegisteredPluginCommand{PluginCommandOwner::PrimeBDS, "rankset"});
        check(result.route == CommandRoute::Existing && result.policy_name == "rankset",
              "Own aliases use the existing canonical command policy and handler");
    }
    for (const auto *label : {"minecraft:paint", "endstone:paint", "primebds:paint", "/PRIMEBDS:Paint"}) {
        check(route(label, external("paint")).route == CommandRoute::Collision,
              "A foreign alias cannot impersonate a trusted command namespace");
        check(route("brush", external(commandLookupName(label))).route == CommandRoute::Collision,
              "A foreign canonical name cannot impersonate a trusted namespace through an alias");
    }
    check(route("minecraft:rank", {RegisteredPluginCommand{PluginCommandOwner::PrimeBDS, "rank"}}).route ==
              CommandRoute::Collision,
          "An own command cannot impersonate the native namespace either");

    for (const auto *label : {"rank", "rset", "mute", "op", "deop", "execute", "function", "time", "tp",
                             "list", "ban", "give", "setblock", "weather", "gamemode", "whisper"}) {
        check(route(label, external("paint")).route == CommandRoute::Collision,
              "Foreign aliases cannot steal an existing protected label");
        check(route("brush", external(label)).route == CommandRoute::Collision,
              "A reserved canonical name cannot bypass its policy through an innocent alias");
    }
    const auto check_native_set = [&](const auto &names) {
        for (const auto &name : names) {
            check(route(name, external("paint")).route == CommandRoute::Collision,
                  "Every current native authorization label is protected against collisions");
        }
    };
    check_native_set(primebds::hierarchy::ordinaryNativeCommands);
    check_native_set(primebds::hierarchy::consoleOnlyNativeCommands);
    check_native_set(primebds::hierarchy::ownerWorldNativeCommands);
    check_native_set(primebds::hierarchy::firstTargetNativeCommands);
    check_native_set(primebds::hierarchy::specialTargetNativeCommands);
    for (const auto *name : {"summon", "locate", "weather", "time"}) {
        check(route(name, external("paint")).route == CommandRoute::Collision,
              "Delegated native world commands retain independent protection");
    }
    auto disabled = external("paint");
    disabled.enabled = false;
    check(route("paint", disabled).route == CommandRoute::Unavailable, "Disabled external owner is not dispatched");
    disabled.enabled = true;
    disabled.registered = false;
    check(route("paint", disabled).route == CommandRoute::Unavailable, "Stale registration is not dispatched");
    auto unsafe = external("paint");
    unsafe.permissions_safe = false;
    check(route("paint", unsafe).route == CommandRoute::UnsafePermissions,
          "A permission that recurses into a cycle cannot reach external dispatch");

    std::cout << "External command ownership, aliases, collisions and unknown-command routing passed.\n";
}
