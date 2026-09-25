#include "primebds/utils/hud_diagnostic.h"
#include "primebds/utils/hud_repair.h"
#include "primebds/utils/database/user_db.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace primebds;
using namespace primebds::utils;

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

static constexpr std::array components{
    HudComponent::Preferences, HudComponent::God, HudComponent::Tasks,
    HudComponent::GameMode, HudComponent::Flying, HudComponent::AllowFlight,
    HudComponent::WalkSpeed, HudComponent::FlySpeed, HudComponent::NameTag
};
static constexpr std::array names{
    "preferences", "god", "tasks", "gamemode", "flying", "allowflight",
    "walkspeed", "flyspeed", "nametag"
};

static HudMask group(std::initializer_list<HudComponent> members) {
    HudMask mask = 0;
    for (const auto component : members) mask |= hudComponentMask(component);
    return mask;
}

static void checkControlsAndSnapshot() {
    HudTestState state;
    check(state.mode() == HudTestMode::Baseline && state.selectedMask() == HudAllComponents,
          "New plugin starts with every component enabled");
    HudMask unique_bits = 0;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto bit = hudComponentMask(components[i]);
        check(bit != 0 && (bit & (bit - 1)) == 0 && !(unique_bits & bit),
              "Each named component must have an independent bit");
        unique_bits |= bit;
        check(std::string(hudComponentName(components[i])) == names[i], "Public component name changed");
    }
    check(unique_bits == HudAllComponents && HudAllComponents == 511, "All nine components are represented");

    const auto baseline = state.beginSync(SyncOrigin::Join);
    const std::vector<std::vector<std::string>> invalid{
        {}, {"other"}, {"skip", "extra"}, {"baseline", "extra"}, {"status", "extra"},
        {"enable"}, {"disable"}, {"enable", "unknown"}, {"disable", "unknown"},
        {"enable", "speeds", "extra"}, {"disable", "walkspeed", "extra"}
    };
    const auto speeds = group({HudComponent::WalkSpeed, HudComponent::FlySpeed});
    const auto flight = group({HudComponent::Flying, HudComponent::AllowFlight});
    check(state.apply(true, {"disable", "speeds"}) == HudTestResult::Changed &&
              state.selectedMask() == (HudAllComponents ^ speeds) && state.mode() == HudTestMode::Custom,
          "Disabling speeds changes only both speed setters and selects custom mode");
    check(state.apply(true, {"disable", "flight"}) == HudTestResult::Changed &&
              state.selectedMask() == (HudAllComponents ^ speeds ^ flight),
          "Group changes accumulate without losing the previous choice");
    check(state.apply(true, {"enable", "walkspeed"}) == HudTestResult::Changed &&
              state.selectedMask() == (HudAllComponents ^ hudComponentMask(HudComponent::FlySpeed) ^ flight),
          "An individual component can override a prior group choice");
    const auto selected = state.selectedMask();
    const auto captured = state.beginSync(SyncOrigin::Join);
    const auto live = state.beginSync(SyncOrigin::Live);
    check(captured.id > baseline.id && live.id > captured.id, "Synchronization IDs increase for both origins");
    for (const auto &args : invalid) {
        check(state.apply(true, args) == HudTestResult::Usage && state.selectedMask() == selected,
              "Invalid console syntax leaves the complete selection untouched");
    }
    auto denied = invalid;
    denied.insert(denied.end(), {{"skip"}, {"baseline"}, {"status"},
                                {"enable", "speeds"}, {"disable", "flight"}});
    for (const auto &args : denied) {
        check(state.apply(false, args) == HudTestResult::Denied && state.selectedMask() == selected,
              "Every in-game request is denied before parsing or mutation");
    }
    check(state.apply(true, {"status"}) == HudTestResult::Status && state.selectedMask() == selected,
          "Status cannot mutate a custom selection");
    check(state.apply(true, {"skip"}) == HudTestResult::Changed && state.selectedMask() == 0 &&
              state.mode() == HudTestMode::Skip, "Skip resets every component, including a custom selection");
    check(captured.selection == selected && captured.mode == HudTestMode::Custom &&
              !captured.skipsReconciliation(), "Later mode changes cannot alter captured selection or mode");
    check(live.selection == selected && live.origin == SyncOrigin::Live && !live.skipsReconciliation(),
          "Live contexts retain selected diagnostics while allowing normal reconciliation");
    check(state.apply(true, {"enable", "speeds"}) == HudTestResult::Changed && state.selectedMask() == speeds,
          "Enabling speeds from skip isolates exactly two setters");
    check(state.apply(true, {"enable", "flight"}) == HudTestResult::Changed && state.selectedMask() == (speeds | flight),
          "Enabling flight includes flying and allow-flight without gamemode");
    check(state.apply(true, {"baseline"}) == HudTestResult::Changed && state.selectedMask() == HudAllComponents &&
              state.mode() == HudTestMode::Baseline, "Baseline replaces a custom selection with all components");
    check(HudTestState{}.selectedMask() == HudAllComponents, "A restart does not retain diagnostic selection");
    check(std::string(hudModeName(HudTestMode::Custom)) == "custom", "Partial selections have a distinct mode name");
}

static void checkEverySelection() {
    // Use the public command parser to construct each of the 512 choices. This
    // catches aliasing between bits, partial-mode mistakes and Live bypass leaks.
    for (HudMask mask = 0; mask <= HudAllComponents; ++mask) {
        HudTestState state;
        state.apply(true, {"skip"});
        std::string enabled, disabled;
        for (std::size_t i = 0; i < components.size(); ++i) {
            auto &list = (mask & hudComponentMask(components[i])) ? enabled : disabled;
            if (!list.empty()) list += ',';
            list += names[i];
            if (mask & hudComponentMask(components[i]))
                check(state.apply(true, {"enable", names[i]}) == HudTestResult::Changed,
                      "Every documented component accepts enable");
        }
        check(hudSelectionList(mask) == (enabled.empty() ? "none" : enabled) &&
                  hudSelectionList(mask, false) == (disabled.empty() ? "none" : disabled),
              "Status lists every selected/unselected component exactly once in stable order");
        check(state.selectedMask() == mask, "Component commands construct the exact selected mask");
        const auto expected_mode = mask == 0 ? HudTestMode::Skip :
            mask == HudAllComponents ? HudTestMode::Baseline : HudTestMode::Custom;
        check(state.mode() == expected_mode, "Mode follows the complete selection, not the last command");
        for (const auto origin : {SyncOrigin::Join, SyncOrigin::Live}) {
            const auto context = state.beginSync(origin);
            check(context.selection == mask && context.mode == expected_mode,
                  "Synchronization captures every component and the selected mode");
            int whole_applied = 0, whole_skipped = 0;
            runHudReconciliation(context, [&] { ++whole_applied; }, [&] { ++whole_skipped; });
            const bool skip_all = origin == SyncOrigin::Join && mask == 0;
            check(context.skipsReconciliation() == skip_all && whole_applied == !skip_all && whole_skipped == skip_all,
                  "Only an empty Join selection bypasses the entire reconciliation");
            for (const auto component : components) {
                const bool allowed = origin == SyncOrigin::Live || (mask & hudComponentMask(component));
                check(context.allows(component) == allowed, "Live permits every component; Join respects each bit");
                for (const bool eligible : {false, true}) {
                    int calls = 0;
                    const auto result = runHudStep(context, component, eligible, [&] { ++calls; });
                    const auto expected = !eligible ? HudStepResult::NotEligible :
                        allowed ? HudStepResult::Executed : HudStepResult::Skipped;
                    check(result == expected && calls == (eligible && allowed),
                          "Each step executes exactly once only if eligible and allowed; ineligibility takes precedence");
                }
            }
        }
        // Disable is independently exercised for every leaf, including idempotence.
        for (std::size_t i = 0; i < components.size(); ++i) {
            const auto before = state.selectedMask();
            check(state.apply(true, {"disable", names[i]}) == HudTestResult::Changed &&
                      state.selectedMask() == (before & ~hudComponentMask(components[i])),
                  "Individual disable clears only its own component");
        }
        check(state.mode() == HudTestMode::Skip, "Disabling every leaf converges on skip mode");
    }

    HudTestState state;
    const auto context = state.beginSync(SyncOrigin::Join);
    bool propagated = false;
    try {
        runHudReconciliation(context, [&] {
            runHudStep(context, HudComponent::WalkSpeed, true, [] { throw std::runtime_error("setter failed"); });
        }, [] {});
    } catch (const std::runtime_error &) {
        propagated = true;
    }
    check(propagated, "A setter failure must not be swallowed and treated as completed reconciliation");
}

// The production sequencing is checked separately in hud_diagnostic_wiring.py.
// This harness exercises real persistent rank/preference operations with the same
// tested gates; it does not pretend to simulate a Bedrock client or reproduce its HUD.
static void checkPersistentCompletion(const std::filesystem::path &directory) {
    const auto path = (directory / "users.db").string();
    {
        db::UserDB database(path);
        database.saveUser("tester", "uuid", "HUD Tester", 1, "os", "device", 1, "version");
        database.assignRank("tester", "DEFAULT");
        check(!database.pendingStateReset("tester") && !database.pendingRankNotice("tester"),
              "Same-rank assignment queues no reconciliation or notice");
        database.updateUser("tester", "enabled_ms", "1");
        database.assignRank("tester", "Moderator");
        check(database.pendingStateReset("tester") && database.pendingRankNotice("tester") == "Moderator",
              "Real rank change queues reconciliation and the final-rank notice");
    }
    {
        db::UserDB database(path);
        HudTestState state;
        state.apply(true, {"skip"});
        bool blocked = true;
        std::vector<std::string> notices;
        int preferences_applied = 0, speeds_applied = 0, full_skips = 0;
        auto synchronize = [&](bool attachment_succeeded, SyncOrigin origin, bool throw_in_setter = false) {
            const auto context = state.beginSync(origin);
            if (!attachment_succeeded) return false;
            if (database.pendingStateReset("tester")) {
                runHudReconciliation(context, [&] {
                    runHudStep(context, HudComponent::Preferences, true, [&] {
                        ++preferences_applied;
                        database.resetUnavailableSettings("tester", {});
                    });
                    runHudStep(context, HudComponent::WalkSpeed, true, [&] {
                        if (throw_in_setter) throw std::runtime_error("setter failed");
                        ++speeds_applied;
                    });
                }, [&] { ++full_skips; });
                database.clearPendingStateReset("tester");
            }
            if (const auto rank = database.pendingRankNotice("tester")) notices.push_back(*rank);
            database.clearPendingRankNotice("tester");
            blocked = false;
            return true;
        };

        check(!synchronize(false, SyncOrigin::Join), "Attachment failure does not complete a sync");
        check(database.pendingStateReset("tester") && database.pendingRankNotice("tester") == "Moderator" &&
                  blocked && notices.empty() && preferences_applied == 0 && full_skips == 0,
              "Failure preserves pending work, notification and command block");
        check(synchronize(true, SyncOrigin::Join), "Fully skipped join synchronization can complete");
        check(preferences_applied == 0 && speeds_applied == 0 && full_skips == 1 &&
                  database.getUserByXuid("tester")->enabled_ms,
              "Full skip suppresses preference and setter work");
        check(!database.pendingStateReset("tester") && !database.pendingRankNotice("tester") &&
                  notices == std::vector<std::string>{"Moderator"} && !blocked,
              "Successful skip consumes work, delivers the notice and unblocks commands");
        synchronize(true, SyncOrigin::Join);
        check(full_skips == 1 && notices.size() == 1, "Completed work and notice are not repeated");

        database.assignRank("tester", "Admin");
        database.assignRank("tester", "Admin");
        check(database.pendingStateReset("tester") && database.pendingRankNotice("tester") == "Admin",
              "Same-rank reassignment preserves work already queued by a real change");
        blocked = true;
        state.apply(true, {"enable", "speeds"});
        check(!synchronize(false, SyncOrigin::Join) && database.pendingStateReset("tester") && blocked,
              "Attachment failure also preserves pending work in a partial mode");
        bool setter_failed = false;
        try {
            synchronize(true, SyncOrigin::Join, true);
        } catch (const std::runtime_error &) {
            setter_failed = true;
        }
        check(setter_failed && database.pendingStateReset("tester") &&
                  database.pendingRankNotice("tester") == "Admin" && blocked && notices.size() == 1,
              "An executing component failure cannot consume pending work, notice or command block");
        check(synchronize(true, SyncOrigin::Join) && speeds_applied == 1 && preferences_applied == 0 &&
                  database.getUserByXuid("tester")->enabled_ms && !database.pendingStateReset("tester") &&
                  !database.pendingRankNotice("tester") && notices.back() == "Admin" && !blocked,
              "Partial success applies selected setters, preserves skipped preferences, acknowledges and unblocks");

        database.assignRank("tester", "Default");
        blocked = true;
        check(synchronize(true, SyncOrigin::Live), "Actual online change recovers during partial mode");
        check(preferences_applied == 1 && speeds_applied == 2 && !database.getUserByXuid("tester")->enabled_ms && !blocked,
              "Live partial-mode reconciliation revokes preferences even though their component is disabled");

        database.updateUser("tester", "enabled_ms", "1");
        database.assignRank("tester", "Moderator");
        state.apply(true, {"skip"});
        check(synchronize(true, SyncOrigin::Live) && preferences_applied == 2 && speeds_applied == 3 &&
                  !database.getUserByXuid("tester")->enabled_ms,
              "Live full-skip mode also executes every eligible component");

        database.assignRank("tester", "Admin");
        database.assignRank("tester", "Moderator");
        check(database.pendingStateReset("tester") && !database.pendingRankNotice("tester"),
              "Restoring the original offline rank remains pending but has no notice");
        state.apply(true, {"baseline"});
        synchronize(true, SyncOrigin::Join);
        check(preferences_applied == 3 && speeds_applied == 4 && !database.pendingStateReset("tester") && notices.size() == 4,
              "Restored baseline executes pending reconciliation without inventing a notice");
    }
    {
        db::UserDB reopened(path);
        check(!reopened.pendingStateReset("tester") && !reopened.pendingRankNotice("tester"),
              "Successful acknowledgements survive a database reopen");
    }
}


// Real SQLite work and the production gate/queue helpers verify acknowledgements,
// silent forced sync and fresh rank reads. Static wiring separately guards the
// Endstone adapter and scheduler; this does not claim to emulate a game client.
static void checkForcedRepairPersistence(const std::filesystem::path &directory) {
    db::UserDB database((directory / "forced-users.db").string());
    database.saveUser("repair", "uuid-repair", "Repair Tester", 1, "os", "device", 1, "version");
    HudTestState state;
    std::vector<std::string> notices, applied_ranks;
    int queued = 0, reconcile_calls = 0;
    bool blocked = true;
    auto synchronize = [&](bool attachment_succeeded, SyncOrigin origin, bool forced = false,
                           bool fallback = false, bool throw_in_setter = false) {
        const auto context = state.beginSync(hudEffectiveSyncOrigin(origin, forced));
        if (fallback) database.assignRank("repair", "Default");
        const auto rank = database.getUserByXuid("repair")->internal_rank;
        if (!attachment_succeeded) return false;
        int executed = 0;
        const auto reconcile = [&] {
            runHudReconciliation(context, [&] {
                runHudStep(context, HudComponent::Preferences, true, [&] {
                    if (throw_in_setter) throw std::runtime_error("setter failed");
                    database.resetUnavailableSettings("repair", {{"primebds.command.socialspy", rank == "Admin"}});
                });
                runHudStep(context, HudComponent::WalkSpeed, true, [&] {
                    if (throw_in_setter) throw std::runtime_error("setter failed");
                    applied_ranks.push_back(rank);
                });
                ++executed;
                ++reconcile_calls;
            }, [] {});
        };
        if (fallback) reconcile();
        if (database.pendingStateReset("repair")) {
            reconcile();
            database.clearPendingStateReset("repair");
        }
        if (forced && executed == 0) reconcile();
        if (!forced) {
            if (const auto notice = database.pendingRankNotice("repair")) notices.push_back(*notice);
        }
        database.clearPendingRankNotice("repair");
        blocked = false;
        if (shouldScheduleHudRepair(origin, forced, executed)) ++queued;
        return true;
    };

    database.assignRank("repair", "DEFAULT");
    check(synchronize(true, SyncOrigin::Join) && queued == 0 && reconcile_calls == 0 && notices.empty(),
          "A clean same-rank join creates neither reconciliation nor a repair");
    database.assignRank("repair", "Admin");
    database.updateUser("repair", "enabled_ss", "1");
    blocked = true;
    check(!synchronize(false, SyncOrigin::Join) && queued == 0 && reconcile_calls == 0 && blocked &&
              database.pendingStateReset("repair") && database.pendingRankNotice("repair") == "Admin",
          "Failed initial permission synchronization queues nothing and preserves pending work and notice");
    check(synchronize(true, SyncOrigin::Join) && queued == 1 && reconcile_calls == 1 && !blocked &&
              !database.pendingStateReset("repair") && notices == std::vector<std::string>{"Admin"},
          "Successful initial reconciliation consumes pending work and queues exactly one repair");
    check(database.getUserByXuid("repair")->enabled_ss, "The initial pass keeps permitted preferences");
    check(synchronize(true, SyncOrigin::Live, true) && queued == 1 && reconcile_calls == 2 &&
              notices.size() == 1 && applied_ranks.back() == "Admin",
          "Forced repair runs after acknowledgement without a recursive task or duplicate notification");

    // An intervening rank update is saved before the delayed pass reads the user.
    database.assignRank("repair", "Default");
    blocked = true;
    state.apply(true, {"skip"});
    check(!synchronize(false, SyncOrigin::Live, true) && blocked && queued == 1 &&
              database.pendingStateReset("repair") && database.pendingRankNotice("repair") == "Default",
          "Failed forced attachment setup preserves newly pending work and its notice");
    bool failed = false;
    try { synchronize(true, SyncOrigin::Live, true, false, true); }
    catch (const std::runtime_error &) { failed = true; }
    check(failed && blocked && queued == 1 && database.pendingStateReset("repair") &&
              database.pendingRankNotice("repair") == "Default" && notices.size() == 1,
          "Forced setter failure cannot acknowledge the new rank or unblock the incomplete synchronization");
    check(synchronize(true, SyncOrigin::Live, true) && !blocked && queued == 1 && reconcile_calls == 3 &&
              applied_ranks.back() == "Default" && !database.getUserByXuid("repair")->enabled_ss &&
              database.getUserByXuid("repair")->internal_rank == "Default" &&
              !database.pendingStateReset("repair") && !database.pendingRankNotice("repair") && notices.size() == 1,
          "Forced repair uses latest rank, applies all components despite skip and silently consumes new work");

    // A Join-origin forced call is also full and silent; it cannot enqueue itself.
    database.assignRank("repair", "Admin");
    check(synchronize(true, SyncOrigin::Join, true) && queued == 1 && reconcile_calls == 4 &&
              applied_ranks.back() == "Admin" && notices.size() == 1,
          "The force contract applies independently of the caller's requested origin");
    database.assignRank("repair", "Moderator");
    check(synchronize(true, SyncOrigin::Join) && queued == 1 && reconcile_calls == 4 &&
              notices.back() == "Moderator" && !database.pendingStateReset("repair"),
          "Full skip consumes ordinary pending work and delivers its notice without scheduling repair");
    database.assignRank("repair", "Admin");
    check(synchronize(true, SyncOrigin::Live) && queued == 1 && reconcile_calls == 5 && notices.back() == "Admin",
          "Actual online changes remain full under skip and never enqueue join repairs");

    // Fallback followed by pending intentionally preserves both original passes.
    state.apply(true, {"enable", "speeds"});
    database.assignRank("repair", "DeletedRank");
    check(synchronize(true, SyncOrigin::Join, false, true) && queued == 2 && reconcile_calls == 7 &&
              notices.back() == "Default" && !database.pendingStateReset("repair"),
          "Both successful fallback and pending sites execute but schedule only one delayed repair");
    check(synchronize(true, SyncOrigin::Live, true) && queued == 2 && reconcile_calls == 8 &&
              notices.back() == "Default" && applied_ranks.back() == "Default",
          "Repair after partial fallback becomes full without inventing another rank notice");
    database.assignRank("repair", "DeletedRank");
    const auto notice_count = notices.size();
    check(synchronize(true, SyncOrigin::Live, true, true) && queued == 2 && reconcile_calls == 10 &&
              notices.size() == notice_count && !database.pendingStateReset("repair") &&
              !database.pendingRankNotice("repair"),
          "Forced fallback preserves two original sites without adding a third invocation or a notice");
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("primebds-hud-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        checkControlsAndSnapshot();
        checkEverySelection();
        checkPersistentCompletion(directory);
        checkForcedRepairPersistence(directory);
        std::filesystem::remove_all(directory);
        std::cout << "HUD controls, all 512 Join/Live component selections, eligibility, persistent completion and silent forced repair tests passed.\n";
    } catch (const std::exception &error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
