"""Verify the Endstone adapter and unchanged .12 reconciliation beside Linux helper/DB tests."""
from hashlib import sha256
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])


def read(path):
    return (root / path).read_text(encoding="utf-8")


def without_comments(source):
    # Keep string literals so console and player messages remain inspectable.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda match: "" if match[0].startswith(("//", "/*")) else match[0], source)


raw_plugin = read("src/plugin.cpp")
start = raw_plugin.index("    void PrimeBDS::reconcilePlayerState(")
end = raw_plugin.index("    std::map<std::string, bool> PrimeBDS::savedPermissions(", start)
# SHA-256 of this exact LF-normalized function section in production cf122ac7.
# This independent baseline protects every original condition, value, call and order.
assert sha256(raw_plugin[start:end].encode("utf-8")).hexdigest() == \
    "161c8c36272f6381b24cb90829f4824af2bacdfc0892fffa771bd5e4afa9d048", \
    "reconcilePlayerState must remain identical to production .12"

plugin = without_comments(raw_plugin)
header = without_comments(read("include/primebds/plugin.h"))
helper = without_comments(read("include/primebds/utils/permission_sync.h"))
join = without_comments(read("src/handlers/connections/join.cpp"))
sync = plugin.split("bool PrimeBDS::reloadCustomPerms(", 1)[1].split("void PrimeBDS::cancelPermissionRepair(", 1)[0]
cancel = plugin.split("void PrimeBDS::cancelPermissionRepair(", 1)[1].split("void PrimeBDS::schedulePermissionRepair(", 1)[0]
repair = plugin.split("void PrimeBDS::schedulePermissionRepair(", 1)[1].split("void PrimeBDS::checkForInactiveSessions(", 1)[0]

assert re.search(r"reloadCustomPerms\(endstone::Player\s*&player,\s*utils::PermissionSyncOrigin\s+origin\s*=\s*utils::PermissionSyncOrigin::Live,\s*bool\s+force_reconcile\s*=\s*false\)", header), \
    "Existing one-argument callers must keep Live origin and opt-in force"
assert "enum class PermissionSyncOrigin { Live, Join };" in helper
assert "PermissionRepairDelayTicks = 20" in helper
assert "std::uint32_t task_id = 0;" in helper, "Scheduler task IDs must not narrow to a signed integer"
assert "bool reconciled = false;" in sync
assert sync.count("reconcilePlayerState(player);") == 3
assert sync.count("reconciled = true;") == 3
assert re.search(r"if\s*\(hierarchy::lower\(internal_rank\) != hierarchy::lower\(user->internal_rank\)\)\s*\{\s*reconcilePlayerState\(player\);\s*reconciled = true;\s*\}", sync)
assert re.search(r"if\s*\(pending\)\s*\{\s*reconcilePlayerState\(player\);\s*reconciled = true;\s*db->clearPendingStateReset\(player.getXuid\(\)\);\s*\}", sync)
assert re.search(r"if\s*\(force_reconcile && !reconciled\)\s*\{\s*reconcilePlayerState\(player\);\s*reconciled = true;\s*\}", sync), \
    "Force guarantees one pass when needed without adding a third fallback/pending invocation"

fallback = sync.index("reconcilePlayerState(player);")
pending = sync.index("if (pending)")
forced = sync.index("if (force_reconcile && !reconciled)")
clear_pending = sync.index("db->clearPendingStateReset(")
notice = sync.index("db->pendingRankNotice(")
clear_notice = sync.index("db->clearPendingRankNotice(")
unblock = sync.index("permissions_pending.erase(")
queue = sync.index("schedulePermissionRepair(")
assert sync.index("player.updateCommands()") < sync.index("player.recalculatePermissions()") < fallback
assert fallback < sync.index("pm.clearPrefixSuffixCache()") < sync.index("pm.invalidatePermCache(") < pending
assert pending < clear_pending < forced < notice < clear_notice < unblock < queue < sync.rindex("return true;")
assert sync.count("db->clearPendingStateReset(") == 1
assert sync.count("db->clearPendingRankNotice(") == 1
assert sync.count("permissions_pending.erase(") == 1
assert re.search(r"if\s*\(!force_reconcile\)\s*\{\s*if\s*\(const auto rank = db->pendingRankNotice\(player.getXuid\(\)\)\)\s*player.sendMessage\(", sync), \
    "Forced reload must never emit the player's rank-change notification"
assert re.search(r'if\s*\(!force_reconcile\)\s*\{\s*if\s*\(const auto rank = db->pendingRankNotice\(player.getXuid\(\)\)\)\s*player.sendMessage\([^;]+;\s*\}\s*db->clearPendingRankNotice\(', sync), \
    "Notification acknowledgement stays outside the silent-force guard"
assert sync.count("player.sendMessage(") == 1, "No additional forced-pass player messages"
assert "Your rank is now" in sync
assert sync.count("schedulePermissionRepair(") == 1
assert re.search(r"if\s*\(utils::shouldSchedulePermissionRepair\(origin, force_reconcile, reconciled\)\)\s*schedulePermissionRepair\(player\);", sync)

failure = sync.index("if (!attachment)")
failure_branch = sync[failure:sync.index("for (const auto &[perm, value]")]
assert re.search(r"return false;\s*\}?\s*$", failure_branch)
for forbidden in ("clearPending", "reconcilePlayerState(", "permissions_pending.erase(", "schedulePermissionRepair("):
    assert forbidden not in failure_branch
assert sync.index("db->getOnlineUser(") < sync.index("savedPermissions(") < failure < fallback
assert "player.setOp(wants_op)" in sync, "The repair must retain native OP synchronization"
print("Unchanged .12 setters, latest-rank reads, original ordering, pending failure preservation and forced silence checked.")

# Only the existing join handler may opt into scheduling the second pass.
join_calls = []
for path in (root / "src").rglob("*.cpp"):
    source = without_comments(path.read_text(encoding="utf-8"))
    for match in re.finditer(r"reloadCustomPerms\([^;]*?PermissionSyncOrigin::Join\)", source):
        join_calls.append(path.relative_to(root).as_posix())
assert join_calls == ["src/handlers/connections/join.cpp"], join_calls
assert "[&plugin, uuid, xuid, session, generation]" in join
assert "plugin.reloadCustomPerms(*p, utils::PermissionSyncOrigin::Join)" in join
join_callback = join.split("[&plugin, uuid, xuid, session, generation]()", 1)[1].split("if (!task)", 1)[0]
lookup = join_callback.index("getPlayer(uuid)")
assert join_callback.index("permission_sessions.generation() != generation") < lookup
assert join_callback.index("!plugin.permission_sessions.active(uuid.str(), session)") < lookup
assert lookup < join_callback.index("!p || !p->isValid()") < join_callback.index("plugin.reloadCustomPerms(*p,")
assert "p->getUniqueId().str() != uuid.str()" in join_callback and "p->getXuid() != xuid" in join_callback
assert "catch (const std::exception &error)" in join_callback and "catch (...)" in join_callback
assert "Could not schedule join permission synchronization" in join

assert "permission_repairs_.take(uuid)" in cancel and "cancelTask(task->task_id)" in cancel
assert repair.count("runTaskLater(") == 1 and "}, utils::PermissionRepairDelayTicks);" in repair
assert "[this, uuid, key, xuid, session, generation, request]" in repair
assert "existing->session == session && existing->generation == generation" in repair
assert "permission_repairs_.nextRequestId()" in repair
assert "permission_repairs_.track(key, {session, generation, request, task->getTaskId()})" in repair
assert "cancelTask(task->getTaskId())" in repair, "Unowned scheduler tasks must be cancelled"
repair_callback = repair.split("[this, uuid, key, xuid, session, generation, request]()", 1)[1].split("}, utils::PermissionRepairDelayTicks);", 1)[0]
lookup = repair_callback.index("getPlayer(uuid)")
assert repair_callback.index("permission_sessions.generation() != generation") < lookup
assert repair_callback.index("!permission_sessions.active(key, session)") < lookup
assert repair_callback.index("permission_repairs_.find(key)") < lookup
assert "tracked->session != session" in repair_callback and "tracked->generation != generation" in repair_callback
assert "tracked->request != request" in repair_callback
assert lookup < repair_callback.index("!p || !p->isValid()") < repair_callback.index("reloadCustomPerms(*p,")
assert "p->getUniqueId().str() != key" in repair_callback and "p->getXuid() != xuid" in repair_callback
assert repair_callback.count("reloadCustomPerms(*p, utils::PermissionSyncOrigin::Live, true)") == 1
assert repair_callback.rindex("permission_repairs_.take(key, session, generation, request)") > repair_callback.index("reloadCustomPerms(*p,")
assert "permission_repairs_.take(key)" not in repair_callback, "Stale callback must never erase newer task ownership"
assert "catch (const std::exception &error)" in repair_callback and "catch (...)" in repair_callback
for forbidden in ("runTaskLaterAsync", "runTaskTimer", "sleep(", "sendMessage(", "setHealth(", "setMaxHealth(", "assignRank(", "dispatchCommand("):
    assert forbidden not in repair, f"Unexpected delayed repair side effect: {forbidden}"
for source in (sync, repair, cancel, join_callback):
    assert not re.search(r"getLogger\(\)\.(?:info|debug|trace)\(", source), \
        "Ordinary repair scheduling, completion and cancellation must be silent"
print("Single synchronous 20-tick repair, fresh lookup, both callback guards and per-task ownership checked.")

login = plugin.split("void EventListener::onPlayerLogin(", 1)[1].split("void EventListener::onPlayerJoin(", 1)[0]
quit = plugin.split("void EventListener::onPlayerQuit(", 1)[1].split("void EventListener::onPlayerKick(", 1)[0]
disable = plugin.split("void PrimeBDS::onDisable()", 1)[1].split("bool PrimeBDS::onCommand(", 1)[0]
assert login.index("cancelPermissionRepair(uuid)") < login.index("permission_sessions.start(uuid)") < login.index("handleLoginEvent(")
assert login.index("handleLoginEvent(") < login.index("if (event.isCancelled())") < login.index("permission_sessions.end(uuid)")
assert login.count("cancelPermissionRepair(uuid)") == 2
assert "permission_sessions.end(uuid)" in quit and "cancelPermissionRepair(uuid)" in quit
assert "permission_sessions.clear()" in disable
assert "for (const auto &uuid : permission_repairs_.uuids()) cancelPermissionRepair(uuid)" in disable

# Scan actual runtime sources and headers, not test expectations mentioning diagnostics.
for directory in ("src", "include"):
    for path in (root / directory).rglob("*"):
        if not path.is_file() or path.suffix not in (".cpp", ".h", ".in"):
            continue
        source = path.read_text(encoding="utf-8")
        assert not re.search(r"hudtest|hud_test|hud_diagnostic|hud_timeline|hud_repair|\[HUDTest\]|HudComponent|logHudState|onHudPacket", source, re.I), \
            f"Diagnostic implementation leaked into production: {path}"
for forbidden in ("getHealth()", "getMaxHealth()", "getWalkSpeed()", "getFlySpeed()", "PacketReceiveEvent", "setPayload("):
    assert forbidden not in sync + repair + join_callback, f"Repair must not introduce snapshots/packet observation: {forbidden}"
assert "3.4.3-chromevale.15" in plugin
print("Disconnect, cancelled login, disable and replacement invalidation checked; production has no diagnostic commands, switches or logging.")
