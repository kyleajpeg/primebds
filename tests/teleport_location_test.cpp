#include "primebds/utils/teleport_location.h"
#include <iostream>
#include <stdexcept>
using namespace primebds::utils;
static void check(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
int main() {
    try {
        auto target = savedDestination(R"({"x":-12.5,"y":70,"z":30,"dimension":"Nether","pitch":15,"yaw":90})");
        check(target && target->dimension=="Nether" && target->x==-12.5 && target->pitch==15 && target->yaw==90, "Saved dimension, coordinates and rotation preserved");
        auto legacy = savedDestination(R"({"x":1,"y":2,"z":3,"dimension":"Overworld"})");
        check(legacy && !legacy->pitch && !legacy->yaw, "Absent legacy rotation must preserve the caller's current rotation");
        for (const auto *bad : {"", "not json", "[]", R"({"x":0,"y":0,"z":0})",
             R"({"x":0,"y":0,"z":0,"dimension":""})", R"({"x":"1","y":2,"z":3,"dimension":"Nether"})",
             R"({"x":1e100,"y":2,"z":3,"dimension":"Nether"})", R"({"x":1,"y":2,"z":3,"dimension":"Nether","pitch":"90"})"})
            check(!savedDestination(bad), "Malformed destination accepted");
        auto logout = logoutDestination("1,2,3", "TheEnd");
        check(logout && logout->dimension=="TheEnd", "Current separate logout dimension column is required");
        check(logoutDestination("1,2,3,Nether", "")->dimension=="Nether", "Legacy inline logout dimension");
        check(logoutDestination("1,2,3,Nether", "Overworld")->dimension=="Overworld", "Separate dimension column takes precedence");
        for (const auto *bad : {"1,2","1,2,3,Nether,extra","nan,2,3","1,inf,3","1,2,3;op","1,,3","1e100,2,3"})
            check(!logoutDestination(bad,"Nether"), "Malformed logout coordinates accepted");
        check(!logoutDestination("1,2,3",""), "Missing dimension must not silently use the current world");

        struct Dimension { std::string name; } overworld{"Overworld"}, nether{"Nether"};
        Dimension *current = &overworld;
        int moves = 0, successes = 0, cooldowns = 0;
        bool allow = true;
        auto resolve = [&](const std::string &name) { return name=="Nether" ? &nether : name=="Overworld" ? &overworld : nullptr; };
        auto move = [&](Dimension &dimension, const TeleportDestination &destination) {
            ++moves;
            if (!allow) return false;
            current = &dimension;
            check(destination.x==-12.5, "Wrong coordinates passed to teleport adapter");
            return true;
        };
        std::string error;
        auto invoke = [&](const std::optional<TeleportDestination> &destination) {
            if (!tryTeleport(destination,resolve,move,error)) return;
            ++successes; ++cooldowns;
        };
        invoke(target);
        check(current==&nether && successes==1 && cooldowns==1 && error.empty(), "Teleport crosses dimension without native command permission checks");
        allow=false;
        invoke(target);
        check(successes==1 && cooldowns==1 && !error.empty(), "Cancelled teleport must not report success or consume cooldown");
        const auto attempts=moves;
        invoke(std::nullopt);
        auto missing=*target; missing.dimension="DeletedDimension";
        invoke(missing);
        check(moves==attempts && successes==1 && cooldowns==1, "Invalid or missing dimension must fail before moving");
        check(!tryTeleport(target,resolve,[](auto &, const auto &) -> bool { throw std::runtime_error("engine failure"); },error) && !error.empty(), "Engine exceptions become a failed teleport");
        std::cout << "Teleport parsing, dimensions and failure contracts passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
