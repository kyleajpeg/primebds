#include "primebds/utils/permission_sync.h"
#include "primebds/utils/database/user_db.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace primebds;
using namespace primebds::utils;

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

// Exercise the same admission policy used by the real scheduler adapter.
static void checkScheduling() {
    check(PermissionRepairDelayTicks == 20, "Join repair waits exactly 20 server ticks");
    for (const auto origin : {PermissionSyncOrigin::Join, PermissionSyncOrigin::Live}) {
        for (const bool forced : {false, true}) {
            for (const bool reconciled : {false, true}) {
                check(shouldSchedulePermissionRepair(origin, forced, reconciled) ==
                          (origin == PermissionSyncOrigin::Join && !forced && reconciled),
                      "Only a successful initial Join with actual reconciliation can enqueue repair");
            }
        }
    }
}

// The standalone test executes real production session/task ownership helpers;
// the wiring check verifies that both Endstone callbacks apply these guards.
static void checkTaskOwnershipAndLifetime() {
    static_assert(std::is_same_v<decltype(PermissionRepairTask::task_id), std::uint32_t>);
    PermissionRepairTasks tasks;
    PermissionSyncSessions sessions;
    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    check(tasks.track("wide", {1, 1, tasks.nextRequestId(), maximum}) &&
              tasks.take("wide")->task_id == maximum,
          "Scheduler task IDs keep all unsigned bits for cancellation");
    const auto first = sessions.start("a");
    const auto other = sessions.start("b");
    const auto generation = sessions.generation();
    const auto first_request = tasks.nextRequestId();
    const auto other_request = tasks.nextRequestId();
    check(first && other > first && other_request > first_request,
          "Session IDs and task ownership IDs increase monotonically");
    const PermissionRepairTask initial{first, generation, first_request, 41};
    const PermissionRepairTask unrelated{other, generation, other_request, 42};
    check(tasks.empty() && !tasks.find("a") && !tasks.take("a"), "New registry has no outstanding repair");
    check(tasks.track("a", initial) && tasks.track("b", unrelated), "Each session may own a repair");
    check(!tasks.track("a", {first, generation, tasks.nextRequestId(), 43}) &&
              tasks.find("a")->task_id == 41 && tasks.find("a")->request == first_request,
          "Duplicate scheduling cannot replace an existing repair");
    check(!tasks.take("a", first + 99, generation, first_request) &&
              !tasks.take("a", first, generation + 1, first_request) &&
              !tasks.take("a", first, generation, other_request) && tasks.find("a"),
          "Stale session, generation and request cannot consume current task ownership");
    const auto completed = tasks.take("a", first, generation, first_request);
    check(completed && completed->task_id == 41 && !tasks.find("a") &&
              !tasks.take("a", first, generation, first_request) && tasks.find("b"),
          "Completion removes exactly its own task and cannot complete twice");

    const auto replacement_request = tasks.nextRequestId();
    check(tasks.track("a", {first, generation, replacement_request, 44}) &&
              !tasks.take("a", first, generation, first_request) && tasks.find("a")->task_id == 44,
          "An old callback cannot erase a newer repair even within the same session");
    sessions.end("a");
    const auto cancelled = tasks.take("a");
    check(!sessions.active("a", first) && cancelled && cancelled->task_id == 44 && !tasks.find("a"),
          "Disconnect invalidates login and returns exact task ID for cancellation");
    const auto reconnect = sessions.start("a");
    const auto reconnect_request = tasks.nextRequestId();
    check(tasks.track("a", {reconnect, generation, reconnect_request, 45}) &&
              reconnect > first && !sessions.active("a", first) && sessions.active("a", reconnect),
          "A rapid reconnect with the same UUID must have distinct login ownership");
    check(!tasks.take("a", first, generation, replacement_request) && tasks.find("a")->task_id == 45,
          "A disconnected callback cannot erase a new login's repair");

    const auto refused = sessions.start("refused");
    sessions.end("refused");
    check(!sessions.active("refused", refused) && !sessions.active("missing", 0),
          "Cancelled and missing logins cannot pass the original join callback's guard");
    const auto replaced = sessions.start("replaced");
    const auto replacement = sessions.start("replaced");
    check(!sessions.active("replaced", replaced) && sessions.active("replaced", replacement),
          "Session replacement invalidates a queued original next-tick callback");

    int cancellations = 0;
    for (const auto &uuid : tasks.uuids()) {
        const auto ticket = tasks.take(uuid);
        check(ticket && (ticket->task_id == 42 || ticket->task_id == 45),
              "Disable retrieves every remaining scheduler task");
        ++cancellations;
    }
    sessions.clear();
    check(cancellations == 2 && tasks.empty() && sessions.generation() != generation &&
              !sessions.active("a", reconnect) && !sessions.active("b", other),
          "Disable empties task bookkeeping and invalidates join and repair callbacks");
    const auto enabled_again = sessions.start("a");
    check(enabled_again > replacement && tasks.nextRequestId() > reconnect_request &&
              sessions.generation() != generation,
          "Re-enable cannot revive previous session, generation or request ownership");
}

// Real SQLite and the production scheduling helper exercise persisted completion.
// The adapter sequencing is checked independently by join_reconciliation_wiring.py;
// this harness does not emulate Endstone or claim to reproduce a Bedrock client.
static void checkPersistentCompletion(const std::filesystem::path &directory) {
    const auto path = (directory / "users.db").string();
    {
        db::UserDB database(path);
        database.saveUser("repair", "uuid-repair", "Repair Tester", 1, "os", "device", 1, "version");
        std::vector<std::string> notices, applied_ranks, trace;
        int queued = 0, reconcile_calls = 0;
        bool blocked = true;
        auto synchronize = [&](bool attachment_succeeded, PermissionSyncOrigin origin, bool forced = false,
                               bool fallback = false, bool throw_in_setter = false) {
            trace.clear();
            if (fallback) database.assignRank("repair", "Default");
            const auto rank = database.getUserByXuid("repair")->internal_rank;
            if (!attachment_succeeded) return false;
            bool reconciled = false;
            const auto reconcile = [&](const char *site) {
                trace.push_back(site);
                if (throw_in_setter) throw std::runtime_error("setter failed");
                database.resetUnavailableSettings("repair", {{"primebds.command.socialspy", rank == "Admin"}});
                applied_ranks.push_back(rank);
                ++reconcile_calls;
                reconciled = true;
            };
            if (fallback) reconcile("fallback");
            trace.push_back("cache");
            if (database.pendingStateReset("repair")) {
                reconcile("pending");
                database.clearPendingStateReset("repair");
                trace.push_back("ack-state");
            }
            if (forced && !reconciled) reconcile("forced");
            if (!forced) {
                if (const auto notice = database.pendingRankNotice("repair")) notices.push_back(*notice);
            }
            database.clearPendingRankNotice("repair");
            trace.push_back("ack-notice");
            blocked = false;
            trace.push_back("unblock");
            if (shouldSchedulePermissionRepair(origin, forced, reconciled)) {
                ++queued;
                trace.push_back("queue");
            }
            return true;
        };

        database.assignRank("repair", "DEFAULT");
        check(!database.pendingStateReset("repair") && !database.pendingRankNotice("repair") &&
                  synchronize(true, PermissionSyncOrigin::Join) && queued == 0 && reconcile_calls == 0 && notices.empty(),
              "Clean same-rank assignment and join create no pending work, notice or repair");
        database.assignRank("repair", "Admin");
        database.assignRank("repair", "Admin");
        database.updateUser("repair", "enabled_ss", "1");
        blocked = true;
        check(!synchronize(false, PermissionSyncOrigin::Join) && queued == 0 && reconcile_calls == 0 && blocked &&
                  database.pendingStateReset("repair") && database.pendingRankNotice("repair") == "Admin",
              "Initial attachment failure preserves queued work and notice and schedules nothing");
        bool failed = false;
        try { synchronize(true, PermissionSyncOrigin::Join, false, false, true); }
        catch (const std::runtime_error &) { failed = true; }
        check(failed && blocked && queued == 0 && database.pendingStateReset("repair") && notices.empty(),
              "Initial reconciliation failure cannot acknowledge pending state or queue a repair");
        check(synchronize(true, PermissionSyncOrigin::Join) && queued == 1 && reconcile_calls == 1 && !blocked &&
                  !database.pendingStateReset("repair") && notices == std::vector<std::string>{"Admin"} &&
                  trace == std::vector<std::string>{"cache", "pending", "ack-state", "ack-notice", "unblock", "queue"},
              "Successful initial reconciliation completes pending work and notifications before queueing once");
        check(database.getUserByXuid("repair")->enabled_ss, "Existing allowed preferences remain enabled");
        check(synchronize(true, PermissionSyncOrigin::Live, true) && queued == 1 && reconcile_calls == 2 &&
                  notices.size() == 1 && applied_ranks.back() == "Admin" &&
                  trace == std::vector<std::string>{"cache", "forced", "ack-notice", "unblock"},
              "Forced repair runs with no pending state, silently and without recursive scheduling");

        database.assignRank("repair", "Default");
        blocked = true;
        check(!synchronize(false, PermissionSyncOrigin::Live, true) && blocked && queued == 1 &&
                  database.pendingStateReset("repair") && database.pendingRankNotice("repair") == "Default",
              "Failed forced attachment preserves new pending work and its notice");
        failed = false;
        try { synchronize(true, PermissionSyncOrigin::Live, true, false, true); }
        catch (const std::runtime_error &) { failed = true; }
        check(failed && blocked && queued == 1 && database.pendingStateReset("repair") &&
                  database.pendingRankNotice("repair") == "Default" && notices.size() == 1,
              "Failed forced reconciliation cannot acknowledge state or silently discard its pending notice");
        check(synchronize(true, PermissionSyncOrigin::Live, true) && !blocked && queued == 1 && reconcile_calls == 3 &&
                  applied_ranks.back() == "Default" && !database.getUserByXuid("repair")->enabled_ss &&
                  database.getUserByXuid("repair")->internal_rank == "Default" &&
                  !database.pendingStateReset("repair") && !database.pendingRankNotice("repair") && notices.size() == 1,
              "Repair reads newest rank, revokes unavailable preferences and silently consumes new work");

        database.assignRank("repair", "Admin");
        check(synchronize(true, PermissionSyncOrigin::Join, true) && queued == 1 && reconcile_calls == 4 &&
                  applied_ranks.back() == "Admin" && notices.size() == 1,
              "Force is silent and nonrecursive even if a caller supplies Join origin");
        database.assignRank("repair", "Moderator");
        check(synchronize(true, PermissionSyncOrigin::Live) && queued == 1 && reconcile_calls == 5 &&
                  notices.back() == "Moderator", "Actual online changes keep ordinary notification and queue no repair");
        check(synchronize(true, PermissionSyncOrigin::Live, true) && queued == 1 && reconcile_calls == 6 &&
                  applied_ranks.back() == "Moderator" && notices.size() == 2,
              "Intervening online rank update does not make delayed repair restore an older rank");

        database.assignRank("repair", "DeletedRank");
        check(synchronize(true, PermissionSyncOrigin::Join, false, true) && queued == 2 && reconcile_calls == 8 &&
                  notices.back() == "Default" && !database.pendingStateReset("repair") &&
                  trace == std::vector<std::string>{"fallback", "cache", "pending", "ack-state", "ack-notice", "unblock", "queue"},
              "Fallback and pending retain two ordered passes but enqueue exactly one repair");
        check(synchronize(true, PermissionSyncOrigin::Live, true) && queued == 2 && reconcile_calls == 9 &&
                  notices.back() == "Default" && applied_ranks.back() == "Default",
              "Acknowledged fallback remains eligible for one silent forced reconciliation");
        database.assignRank("repair", "DeletedRank");
        const auto notice_count = notices.size();
        check(synchronize(true, PermissionSyncOrigin::Live, true, true) && queued == 2 && reconcile_calls == 11 &&
                  notices.size() == notice_count && !database.pendingStateReset("repair") &&
                  !database.pendingRankNotice("repair") &&
                  trace == std::vector<std::string>{"fallback", "cache", "pending", "ack-state", "ack-notice", "unblock"},
              "Forced fallback preserves both original sites without adding a third pass or notification");

        database.assignRank("repair", "Admin");
        database.assignRank("repair", "Default");
        check(database.pendingStateReset("repair") && !database.pendingRankNotice("repair"),
              "Offline round trip retains reconciliation work without inventing a final-rank notice");
        check(synchronize(true, PermissionSyncOrigin::Join) && queued == 3 && reconcile_calls == 12 &&
                  notices.size() == notice_count && !database.pendingStateReset("repair"),
              "Offline round-trip pending work still gets one delayed repair");
    }
    db::UserDB reopened(path);
    check(!reopened.pendingStateReset("repair") && !reopened.pendingRankNotice("repair") &&
              reopened.getUserByXuid("repair")->internal_rank == "Default",
          "Successful state and notification acknowledgements survive a database reopen");
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("primebds-join-reconciliation-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        checkScheduling();
        checkTaskOwnershipAndLifetime();
        checkPersistentCompletion(directory);
        std::filesystem::remove_all(directory);
        std::cout << "Production join repair criteria, task lifetime, persisted completion and silent forced reconciliation passed.\n";
    } catch (const std::exception &error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
