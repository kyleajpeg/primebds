#include "primebds/utils/hud_diagnostic.h"
#include "primebds/utils/database/user_db.h"

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

static void checkControlsAndGate() {
    HudTestState state;
    check(state.mode() == HudTestMode::Baseline, "New plugin starts in baseline mode");
    const auto first = state.beginSync(SyncOrigin::Join);
    check(first.origin == SyncOrigin::Join && first.mode == HudTestMode::Baseline,
          "Synchronization captures its origin and mode");
    for (const auto &args : std::vector<std::vector<std::string>>{
            {"skip"}, {"baseline"}, {"status"}, {}, {"skip", "extra"}}) {
        check(state.apply(false, args) == HudTestResult::Denied,
              "All in-game requests are denied, including status and invalid syntax");
        check(state.mode() == HudTestMode::Baseline, "Denied request cannot change baseline mode");
    }
    for (const auto &args : std::vector<std::vector<std::string>>{
            {}, {"other"}, {"skip", "extra"}}) {
        check(state.apply(true, args) == HudTestResult::Usage, "Console syntax requires one valid action");
        check(state.mode() == HudTestMode::Baseline, "Invalid command cannot change the mode");
    }
    check(state.apply(true, {"status"}) == HudTestResult::Status, "Console can request status");
    check(state.apply(true, {"skip"}) == HudTestResult::Changed, "Console can select skip mode");
    const auto skipped_join = state.beginSync(SyncOrigin::Join);
    const auto live = state.beginSync(SyncOrigin::Live);
    check(skipped_join.id > first.id && live.id > skipped_join.id, "Sync IDs increase for both origins");
    check(state.apply(false, {"baseline"}) == HudTestResult::Denied && state.mode() == HudTestMode::Skip,
          "In-game sender cannot disable skip mode either");
    check(state.apply(true, {"status"}) == HudTestResult::Status && state.mode() == HudTestMode::Skip,
          "Status does not change the selected mode");
    check(state.apply(true, {"baseline"}) == HudTestResult::Changed, "Console can restore baseline");
    check(skipped_join.mode == HudTestMode::Skip && skipped_join.skipsReconciliation(),
          "Mode changes cannot alter an already captured synchronization");
    check(!first.skipsReconciliation() && !live.skipsReconciliation(),
          "Baseline joins and live synchronizations are not bypassed");
    check(HudTestState{}.mode() == HudTestMode::Baseline, "A new plugin instance does not retain diagnostic mode");

    for (const auto mode : {HudTestMode::Baseline, HudTestMode::Skip}) {
        for (const auto origin : {SyncOrigin::Join, SyncOrigin::Live}) {
            HudTestState matrix_state;
            matrix_state.apply(true, {mode == HudTestMode::Skip ? "skip" : "baseline"});
            const auto context = matrix_state.beginSync(origin);
            int applies = 0, skips = 0;
            runHudReconciliation(context, [&] { ++applies; }, [&] { ++skips; });
            const bool should_skip = origin == SyncOrigin::Join && mode == HudTestMode::Skip;
            check(applies == (should_skip ? 0 : 1) && skips == (should_skip ? 1 : 0),
                  "Exactly one callback runs; only skip-mode joins bypass reconciliation");
        }
    }
    bool propagated = false;
    try {
        runHudReconciliation(first, [] { throw std::runtime_error("setter failed"); }, [] {});
    } catch (const std::runtime_error &) {
        propagated = true;
    }
    check(propagated, "Reconciliation failure cannot be swallowed and treated as successful synchronization");
}

// Use the same gate and the real persistent DB operations as reloadCustomPerms.
// The wiring regression separately checks that the production function keeps this
// success/failure ordering; this harness does not pretend to simulate a Bedrock HUD.
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
        int applied = 0, skipped = 0;
        auto synchronize = [&](bool attachment_succeeded, SyncOrigin origin) {
            const auto context = state.beginSync(origin);
            if (!attachment_succeeded) return false;
            if (database.pendingStateReset("tester")) {
                runHudReconciliation(context, [&] {
                    ++applied;
                    database.resetUnavailableSettings("tester", {});
                }, [&] { ++skipped; });
                database.clearPendingStateReset("tester");
            }
            if (const auto rank = database.pendingRankNotice("tester")) notices.push_back(*rank);
            database.clearPendingRankNotice("tester");
            blocked = false;
            return true;
        };

        check(!synchronize(false, SyncOrigin::Join), "Attachment failure does not complete a sync");
        check(database.pendingStateReset("tester") && database.pendingRankNotice("tester") == "Moderator" &&
                  blocked && notices.empty() && applied == 0 && skipped == 0,
              "Failure preserves pending work, notification and command block");
        check(synchronize(true, SyncOrigin::Join), "Skipped join synchronization can complete successfully");
        check(applied == 0 && skipped == 1 && database.getUserByXuid("tester")->enabled_ms,
              "Skip mode suppresses the actual persisted preference reset");
        check(!database.pendingStateReset("tester") && !database.pendingRankNotice("tester") &&
                  notices == std::vector<std::string>{"Moderator"} && !blocked,
              "Successful skip consumes work, delivers the notice and unblocks commands");
        synchronize(true, SyncOrigin::Join);
        check(skipped == 1 && notices.size() == 1, "Completed work and notice are not repeated");

        database.assignRank("tester", "Default");
        blocked = true;
        check(synchronize(true, SyncOrigin::Live), "Actual online change can recover during skip mode");
        check(applied == 1 && skipped == 1 && !database.getUserByXuid("tester")->enabled_ms && !blocked,
              "Live reconciliation still revokes unavailable settings");

        database.assignRank("tester", "Admin");
        database.assignRank("tester", "Default");
        check(database.pendingStateReset("tester") && !database.pendingRankNotice("tester"),
              "Restoring the original offline rank remains pending but has no notice");
        state.apply(true, {"baseline"});
        synchronize(true, SyncOrigin::Join);
        check(applied == 2 && !database.pendingStateReset("tester") && notices.size() == 2,
              "Restored baseline executes pending reconciliation without inventing a notice");
    }
    {
        db::UserDB reopened(path);
        check(!reopened.pendingStateReset("tester") && !reopened.pendingRankNotice("tester"),
              "Successful acknowledgements survive a database reopen");
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("primebds-hud-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        checkControlsAndGate();
        checkPersistentCompletion(directory);
        std::filesystem::remove_all(directory);
        std::cout << "HUD diagnostic controls, gate and persistent completion tests passed.\n";
    } catch (const std::exception &error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
