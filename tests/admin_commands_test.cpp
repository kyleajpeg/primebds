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

        using Mode = SpeedRequest::Mode;
        const auto reset = parseSpeed({"reset"});
        check(reset && reset->reset && reset->mode == Mode::Both && reset->target.empty(), "Bare reset restores both speeds");
        const auto target = parseSpeed({"reset", "Player Name"});
        check(target && target->mode == Mode::Both && target->target == "Player Name", "Targeted reset");
        const auto walk = parseSpeed({"walkspeed", "reset", "Alice"});
        check(walk && walk->reset && walk->mode == Mode::Walk && walk->target == "Alice", "Mode-first reset");
        const auto fly = parseSpeed({"reset", "flyspeed", "Alice"});
        check(fly && fly->reset && fly->mode == Mode::Fly && fly->target == "Alice", "Reset-first mode");
        const auto value = parseSpeed({"walkspeed", "0.1", "Alice"});
        check(value && !value->reset && value->value == 0.1f, "Explicit speed value");
        check(parseSpeed({"0"}).has_value(), "Zero speed is valid");
        for (const auto &bad : {"nan", "inf", "-1", "1e100", "1garbage", "", "resetty"})
            check(!parseSpeed({bad}), "Invalid speed rejected");
        check(!parseSpeed({"reset", "Alice", "Bob"}), "Ambiguous reset rejected");
        check(!parseSpeed({"unknown", "1"}), "Unknown speed mode rejected");
        check(!parseSpeed({}), "Missing speed rejected");

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
