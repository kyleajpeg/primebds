#include "primebds/utils/admin_commands.h"
#include <iostream>
#include <stdexcept>

using namespace primebds::utils;
static void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
struct FakeUuid { std::string value; std::string str() const { return value; } };
struct FakePlayer {
    FakeUuid uuid; int runtime_id;
    int health = 4, maximum = 40, hunger = 3;
    bool valid = true;
    bool isValid() const { return valid; }
    int getHealth() const { return health; }
    int getMaxHealth() const { return maximum; }
    void setHealth(int value) { health = value; }
    FakeUuid getUniqueId() const { return uuid; }
    int getRuntimeId() const { return runtime_id; }
};
int main() {
    try {
        GodModeState god;
        FakePlayer original{{"alice-uuid"}, 42}, respawn{{"alice-uuid"}, 99}, other{{"bob-uuid"}, 42};
        check(!god.enabled(original), "God mode starts disabled");
        god.set(original, true);
        check(god.enabled(original), "Enable protects original player");
        check(god.enabled(respawn), "Respawn runtime ID change preserves protection");
        check(!god.enabled(other), "Reused runtime ID does not grant protection to another player");
        god.set(respawn, false);
        check(!god.enabled(original), "Disable and leave clear UUID state");

        int pulses = 0;
        auto feed = [&]() { original.hunger = 20; ++pulses; };
        god.set(original, true);
        check(maintainGodVitals(god, original, feed) && original.health == 40 && original.hunger == 20 && pulses == 1,
            "God enable restores actual max health and refills hunger");
        original.health = 7; original.hunger = 12;
        maintainGodVitals(god, original, feed);
        check(original.health == 40 && original.hunger == 20 && pulses == 2, "Repeated maintenance keeps bars full");
        original.health = 11;
        maintainGodVitals(god, original, feed, false);
        check(original.health == 40 && pulses == 2, "Health checked every tick without feeding every tick");
        god.set(original, false);
        original.health = 5; original.hunger = 2;
        check(!maintainGodVitals(god, original, feed) && original.health == 5 && original.hunger == 2,
            "Disable or permission revocation stops both refills");
        god.set(original, true); original.health = 0;
        check(!maintainGodVitals(god, original, feed) && original.health == 0, "Maintenance never resurrects dead actors");
        original.health = 4; original.valid = false;
        check(!maintainGodVitals(god, original, feed), "Disconnected actor not modified");
        original.valid = true;
        for (float multiplier : {0.0f,0.5f,1.0f,1.5f,2.0f,3.0f}) {
            check(std::abs(rawSpeed(multiplier,false) - 0.1f*multiplier) < 0.000001f, "Walk multiplier API conversion");
            check(std::abs(rawSpeed(multiplier,true) - 0.05f*multiplier) < 0.000001f, "Fly multiplier API conversion");
            check(std::abs(speedMultiplier(rawSpeed(multiplier,false),false) - multiplier) < 0.000001f,
                "Reported multiplier matches requested value");
        }
        check(rawSpeed(1,false) == NormalWalkSpeed && rawSpeed(1,true) == NormalFlySpeed,
            "Reset and multiplier 1 have identical normal speed");

        using Mode = SpeedRequest::Mode;
        const auto reset = parseSpeed({"reset"});
        check(reset && reset->reset && reset->mode == Mode::Both && reset->target.empty(), "Bare reset restores both speeds");
        const auto target = parseSpeed({"reset", "Player Name"});
        check(target && target->mode == Mode::Both && target->target == "Player Name", "Targeted reset");
        const auto walk = parseSpeed({"walkspeed", "reset", "Alice"});
        check(walk && walk->reset && walk->mode == Mode::Walk && walk->target == "Alice", "Mode-first reset");
        const auto fly = parseSpeed({"reset", "flyspeed", "Alice"});
        check(fly && fly->reset && fly->mode == Mode::Fly && fly->target == "Alice", "Reset-first mode");
        const auto value = parseSpeed({"walkspeed", "1.5", "Alice"});
        check(value && !value->reset && value->value == 1.5f, "Fractional speed multiplier");
        const auto shorthand = parseSpeed({"2","Il Gallon lI"});
        check(shorthand && shorthand->target == "Il Gallon lI" && shorthand->value == 2, "Targeted numeric shorthand with spaced gamertag");
        const auto targetedFly = parseSpeed({"flyspeed","2","Il Gallon lI"});
        check(targetedFly && targetedFly->mode == Mode::Fly && targetedFly->target == "Il Gallon lI", "Explicit targeted fly speed");
        int modeEnums = 0;
        for (const auto &usage : speedUsages()) if (usage.find("speed_mode") != std::string::npos) ++modeEnums;
        check(modeEnums == 1, "No duplicate speed mode autocomplete enum");
        check(parseSpeed({"0"}).has_value(), "Zero speed is valid");
        for (const auto &bad : {"nan", "inf", "-1", "1e100", "1garbage", "", "resetty"})
            check(!parseSpeed({bad}), "Invalid speed rejected");
        check(!parseSpeed({"reset", "Alice", "Bob"}), "Ambiguous reset rejected");
        check(!parseSpeed({"unknown", "1"}), "Unknown speed mode rejected");
        const auto query = parseSpeed({});
        check(query && query->query && !query->reset, "Bare speed queries without modifying abilities");

        auto activity = parseActivityList({});
        check(activity && activity->page == 1 && activity->filter == "highest", "Activity defaults");
        activity = parseActivityList({"recent", "2"});
        check(activity && activity->page == 2 && activity->filter == "recent", "Filter first");
        activity = parseActivityList({"2", "lowest"});
        check(activity && activity->page == 2 && activity->filter == "lowest", "Page first");
        check(parseActivityList({"recent"}).has_value(), "Filter alone");
        for (const auto &bad : {"0", "-1", "999999999999999", "2x", "unknown"})
            check(!parseActivityList({bad}), "Invalid activity page/filter rejected");
        check(!parseActivityList({"1", "unknown"}), "Invalid second filter rejected");
        check(clearsNickname({}) && clearsNickname({"remove", "Alice"}) && clearsNickname({"reset"}), "Nickname removal forms");
        check(!clearsNickname({"kdog"}), "Ordinary nickname is retained");
        check(feedCommand("Player Name") == "effect \"Player Name\" saturation 1 19 true", "Feed quotes spaced player names");
        check(feedCommand("A\"B\\C") == "effect \"A\\\"B\\\\C\" saturation 1 19 true", "Feed escapes command argument");
        std::cout << "Admin command regression tests passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
