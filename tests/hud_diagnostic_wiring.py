"""Guard the live integration that the standalone HUD policy test cannot execute."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])


def read(path):
    return (root / path).read_text(encoding="utf-8")


def without_comments(source):
    # Preserve quoted C++ strings (including log messages) while dropping comments.
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda match: "" if match[0].startswith(("//", "/*")) else match[0], source)


plugin = without_comments(read("src/plugin.cpp"))
sync = plugin.split("bool PrimeBDS::reloadCustomPerms(", 1)[1].split("void PrimeBDS::checkForInactiveSessions(", 1)[0]
header = without_comments(read("include/primebds/plugin.h"))
join = without_comments(read("src/handlers/connections/join.cpp"))
command = without_comments(read("src/commands/server/hudtest.cpp"))

assert re.search(r"reloadCustomPerms\(endstone::Player\s*&player,\s*utils::SyncOrigin\s+origin\s*=\s*utils::SyncOrigin::Live\)", header), \
    "Existing callers must keep the Live default"
assert "utils::SyncOrigin origin" in sync
assert "beginSync(origin)" in sync, "Capture diagnostic mode once per synchronization"
assert sync.count("beginSync(") == 1

# Only the UUID-safe, existing join callback selects the diagnostic bypass origin.
join_origin_calls = []
for path in (root / "src").rglob("*.cpp"):
    source = without_comments(path.read_text(encoding="utf-8"))
    for match in re.finditer(r"reloadCustomPerms\([^;]*?SyncOrigin::Join\)", source):
        join_origin_calls.append(path.relative_to(root).as_posix())
assert join_origin_calls == ["src/handlers/connections/join.cpp"], join_origin_calls
assert "[&plugin, uuid]" in join and "getPlayer(uuid)" in join
assert re.search(r"if\s*\(p\)\s*plugin.reloadCustomPerms\(\*p,\s*utils::SyncOrigin::Join\)", join)

# Both .12 reconciliation sites must use the same gate, preserving their ordering.
assert sync.count("runHudReconciliation(") == 1, "Use one shared gate for both reconciliation sites"
assert sync.count("reconcilePlayerState(") == 1, "No direct call may bypass the shared diagnostic gate"
assert sync.count('diagnosticReconcile("fallback")') == 1
assert sync.count('diagnosticReconcile("pending")') == 1
fallback = sync.index('diagnosticReconcile("fallback")')
pending = sync.index('diagnosticReconcile("pending")')
assert sync.index("player.updateCommands()") < sync.index("player.recalculatePermissions()") < fallback
assert fallback < sync.index("pm.clearPrefixSuffixCache()") < sync.index("pm.invalidatePermCache(") < pending
clear_pending = sync.index("db->clearPendingStateReset(")
notice = sync.index("db->pendingRankNotice(")
clear_notice = sync.index("db->clearPendingRankNotice(")
unblock = sync.index("permissions_pending.erase(")
assert pending < clear_pending < notice < clear_notice < unblock
assert sync.count("db->clearPendingStateReset(") == 1
assert sync.count("db->clearPendingRankNotice(") == 1
assert sync.count("permissions_pending.erase(") == 1
assert re.search(r"if\s*\(pending\)\s*\{\s*diagnosticReconcile\(\"pending\"\);\s*db->clearPendingStateReset", sync), \
    "Skipped work must still reach the original acknowledgement"

# Failed attachment setup must return before any reconcile or acknowledgement.
failure = sync.index("if (!attachment)")
failure_branch = sync[failure:sync.index("for (const auto &[perm, value]")]
assert re.search(r"return false;\s*\}?\s*$", failure_branch), \
    "Attachment failure must leave work pending"
for forbidden in ("clearPending", "diagnosticReconcile(", "permissions_pending.erase("):
    assert forbidden not in failure_branch
assert failure < fallback < pending < clear_pending
assert "player.setOp(wants_op)" in sync, "Do not accidentally bypass native OP synchronization"

# Console-only means the actual console identity, not Owner/OP or !asPlayer().
assert "hierarchy::isConsole(plugin, sender)" in command
assert re.search(r"\.apply\(\s*hierarchy::isConsole\(plugin, sender\),\s*args\)", command), \
    "The tested admission gate must receive the actual sender identity"
assert "HudTestResult::Denied" in command
for forbidden in ("reloadCustomPerms", "reconcilePlayerState", "dispatchCommand", "runTask", "setOp(", "assignRank("):
    assert forbidden not in command, f"Diagnostic mode command unexpectedly mutates players: {forbidden}"

metadata = read("src/commands/command_metadata.cpp")
assert 'cmd(b, "hudtest")' in metadata and "REGISTER_COMMAND(hudtest," in command
for source in (command, metadata.split('cmd(b, "hudtest")', 1)[1].split("cmd(b,", 1)[0]):
    assert "baseline|skip|status" in source, "Console controls must expose the intended three actions"
assert "[HUDTest]" in plugin and "getHealth()" in plugin, "Diagnostics must include snapshots of server health"
workflow = read(".github/workflows/build.yml")
assert "branches: [chromevale-permissions-fix, chromevale-health-hud-test]" in workflow
print("HUD join-only bypass, both reconciliation sites, failure/completion order and console wiring checked.")
