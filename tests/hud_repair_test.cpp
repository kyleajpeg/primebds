#include "primebds/utils/hud_repair.h"
#include "primebds/utils/hud_timeline.h"

#include <iostream>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <string>

using namespace primebds::utils;

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

static void checkSchedulingAndForce() {
    check(HudRepairDelayTicks == 20, "Repair delay is exactly 20 server ticks");
    for (const auto origin : {SyncOrigin::Join, SyncOrigin::Live}) {
        for (const bool forced : {false, true}) {
            for (const int executed : {0, 1, 2}) {
                check(shouldScheduleHudRepair(origin, forced, executed) ==
                          (origin == SyncOrigin::Join && !forced && executed > 0),
                      "Only an initial Join with executed reconciliation qualifies for one repair");
            }
            for (HudMask mask = 0; mask <= HudAllComponents; ++mask) {
                HudTestState state;
                state.apply(true, {"skip"});
                for (const auto component : HudComponents)
                    if ((mask & hudComponentMask(component)) != 0)
                        state.apply(true, {"enable", hudComponentName(component)});
                const auto effective_origin = hudEffectiveSyncOrigin(origin, forced);
                const auto context = state.beginSync(effective_origin);
                check(context.selection == mask && context.mode == hudSelectionMode(mask),
                      "A forced context still captures the current diagnostic selection for logging");
                check(effective_origin == (forced ? SyncOrigin::Live : origin),
                      "Force uses normal full reconciliation without changing ordinary origins");
                int executed = 0, skipped = 0;
                runHudReconciliation(context, [&] { ++executed; }, [&] { ++skipped; });
                const bool full_skip = !forced && origin == SyncOrigin::Join && mask == 0;
                check(executed == !full_skip && skipped == full_skip,
                      "Forced reconciliation never takes the full-skip path for any selection");
                for (const auto component : HudComponents) {
                    for (const bool eligible : {false, true}) {
                        int calls = 0;
                        const auto outcome = runHudStep(context, component, eligible, [&] { ++calls; });
                        const bool enabled = forced || origin == SyncOrigin::Live ||
                            (mask & hudComponentMask(component)) != 0;
                        const auto expected = !eligible ? HudStepResult::NotEligible :
                            enabled ? HudStepResult::Executed : HudStepResult::Skipped;
                        check(outcome == expected && calls == (eligible && enabled),
                              "Forced repair enables every eligible component but never forces ineligible setters");
                    }
                }
                check(!forced || !shouldScheduleHudRepair(origin, forced, executed),
                      "Forced synchronization cannot schedule itself recursively");
            }
        }
    }
}

static void checkTaskOwnershipAndLifetime() {
    static_assert(std::is_same_v<decltype(HudRepairTask::task_id), std::uint32_t>);
    HudRepairTasks wide_ids;
    const auto largest_id = std::numeric_limits<std::uint32_t>::max();
    check(wide_ids.track("wide", {1, 1, 1, 0, largest_id}) &&
              wide_ids.take("wide")->task_id == largest_id,
          "Scheduler task IDs retain their full unsigned width for cancellation");
    HudTimeline sessions;
    HudRepairTasks tasks;
    const auto first = sessions.start("a");
    const auto other = sessions.start("b");
    const auto generation = sessions.generation();
    const HudRepairTask initial{first, generation, 7, 100, 41};
    const HudRepairTask unrelated{other, generation, 8, 101, 42};
    check(tasks.empty() && !tasks.find("a") && !tasks.take("a"), "New repair registry is empty");
    check(tasks.track("a", initial) && tasks.track("b", unrelated), "Independent sessions can each own a task");
    check(!tasks.track("a", HudRepairTask{first, generation, 9, 103, 43}) &&
              tasks.find("a")->task_id == 41 && tasks.find("a")->parent_sync == 7,
          "Duplicate scheduling cannot replace an existing repair or change its correlation");
    check(!tasks.take("a", first + 99, generation, 7) &&
              !tasks.take("a", first, generation + 1, 7) &&
              !tasks.take("a", first, generation, 9) && tasks.find("a"),
          "A stale callback cannot consume a different session, plugin generation, or synchronization task");

    const auto completed = tasks.take("a", first, generation, 7);
    check(completed && completed->queued_ms == 100 && completed->task_id == 41 &&
              !tasks.find("a") && !tasks.take("a", first, generation, 7) && tasks.find("b"),
          "Successful callback takes its bookkeeping exactly once without affecting another player");

    check(tasks.track("a", initial), "The registry can track a later task after prior completion");
    sessions.end("a");
    const auto cancelled = tasks.take("a");
    check(!sessions.active("a", first) && cancelled && cancelled->task_id == 41 && !tasks.find("a"),
          "Disconnect invalidates the login and returns the exact scheduler task to cancel");
    const auto reconnect = sessions.start("a");
    const HudRepairTask replacement{reconnect, sessions.generation(), 10, 200, 44};
    check(tasks.track("a", replacement) && !sessions.active("a", first) && sessions.active("a", reconnect),
          "Rapid reconnect with the same UUID belongs to a different login session");
    check(!tasks.take("a", first, generation, 7) && tasks.find("a")->task_id == 44,
          "Late completion from the old login cannot erase the reconnect's repair");

    // Cancelled login takes the same expiry route as disconnect before the join callback.
    const auto refused = sessions.start("refused");
    sessions.end("refused");
    check(!sessions.active("refused", refused) && !tasks.find("refused"),
          "A cancelled login cannot satisfy the original next-tick callback session guard");

    const auto before_disable = sessions.generation();
    sessions.clear();
    int cancellations = 0;
    for (const auto &uuid : tasks.uuids()) {
        const auto ticket = tasks.take(uuid);
        check(ticket && (ticket->task_id == 42 || ticket->task_id == 44),
              "Plugin shutdown obtains every outstanding scheduler task");
        ++cancellations;
    }
    check(cancellations == 2 && tasks.empty() && sessions.generation() != before_disable &&
              !sessions.active("a", reconnect) && !sessions.active("b", other),
          "Plugin disable clears task ownership and invalidates both pending join and repair callbacks");
    const auto enabled_again = sessions.start("a");
    check(enabled_again != reconnect && sessions.generation() != replacement.generation,
          "Plugin re-enable does not revive prior session or generation callbacks");
}

int main() {
    try {
        checkSchedulingAndForce();
        checkTaskOwnershipAndLifetime();
        std::cout << "HUD repair queue criteria, all 512 forced Join/Live selections and session task ownership passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
