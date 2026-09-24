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
reconcile = plugin.split("void PrimeBDS::reconcilePlayerState(", 1)[1].split("std::map<std::string, bool> PrimeBDS::savedPermissions(", 1)[0]

assert re.search(r"reloadCustomPerms\(endstone::Player\s*&player,\s*utils::SyncOrigin\s+origin\s*=\s*utils::SyncOrigin::Live\)", header), \
    "Existing callers must keep the Live default"
assert "utils::SyncOrigin origin" in sync
assert "beginSync(origin)" in sync, "Capture diagnostic mode once per synchronization"
assert sync.count("beginSync(") == 1
assert "context.selection" in sync and "selection_mask=" in sync and "effective=" in sync
assert "hud_test = utils::HudTestState{}" in plugin, "Restart must restore the complete baseline selection"

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

# The pure helper tests every mask; verify real mutations cannot bypass its gate.
# Parenthesis matching preserves callback bodies even when they contain nested calls.
def calls(source, function):
    masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda match: " " * len(match[0]), source)
    result = []
    for match in re.finditer(r"\b" + re.escape(function) + r"\s*\(", masked):
        depth = 1
        end = match.end()
        while depth:
            assert end < len(masked), f"Unbalanced {function} call"
            depth += (masked[end] == "(") - (masked[end] == ")")
            end += 1
        result.append(source[match.start():end])
    return result


steps = calls(reconcile, "diagnosticStep")
assert reconcile.count("runHudStep(") == 1
assert "runHudStep(context, component, eligible," in reconcile
assert "context.allows(component)" in reconcile and "hudStepResultName(result)" in reconcile
assert "hud_test" not in reconcile, "Each operation must use the captured context, not mutable global controls"
assert 'logHudState(player, context, "before", reason)' in reconcile
assert 'logHudState(player, context, "after", reason)' in reconcile

for component, mutation, count in (
    ("Preferences", "db->resetUnavailableSettings(", 1),
    ("Preferences", "afk_cache.erase(", 1),
    ("God", "isgod.set(", 1),
    ("Tasks", "cancelTask(", 1),
    ("Tasks", "intervals->erase(", 1),
    ("GameMode", "player.setGameMode(", 1),
    ("Flying", "player.setFlying(", 2),
    ("AllowFlight", "player.setAllowFlight(", 2),
    ("WalkSpeed", "player.setWalkSpeed(", 1),
    ("FlySpeed", "player.setFlySpeed(", 1),
    ("NameTag", "player.setNameTag(", 1),
):
    guarded = [step for step in steps if step.startswith(f"diagnosticStep(HudComponent::{component},")]
    assert reconcile.count(mutation) == count, f"Unexpected mutation count: {mutation}"
    assert sum(step.count(mutation) for step in guarded) == count, f"Ungated/wrong-component mutation: {mutation}"

# Both flight branches and the unconditional speed calls keep .12 ordering.
assert re.findall(r"player\.(set\w+)\(", reconcile) == [
    "setGameMode", "setFlying", "setAllowFlight", "setAllowFlight", "setFlying",
    "setWalkSpeed", "setFlySpeed", "setNameTag",
], "Baseline setter ordering or number of call sites changed"
assert reconcile.index("db->resetUnavailableSettings(") < reconcile.index("isgod.set(") < \
       reconcile.index("afk_cache.erase(") < reconcile.index("cancelTask(") < reconcile.index("player.setGameMode(")
for component, setter in (("WalkSpeed", "setWalkSpeed"), ("FlySpeed", "setFlySpeed")):
    assert re.search(rf'diagnosticStep\(HudComponent::{component},\s*"{setter}",\s*true,', reconcile), \
        "Speed calls must remain eligible even when old and requested values match"
assert 'const auto requested_walk = has("speed") ? walk_speed : utils::NormalWalkSpeed;' in reconcile
assert 'const auto requested_fly = has("speed") ? fly_speed : utils::NormalFlySpeed;' in reconcile
assert 'const auto current_mode = player.getGameMode();' in reconcile
assert reconcile.index("player.setGameMode(") < reconcile.index("const auto current_mode") < reconcile.index("player.setFlying(")
assert re.search(r'if\s*\(!has\("fly"\)\s*&&\s*current_mode != endstone::GameMode::Creative\s*&&\s*current_mode != endstone::GameMode::Spectator\)', reconcile)
assert 'else if (has("fly") && mode != current_mode)' in reconcile, \
    "Flight eligibility must use the actual resulting mode, including a skipped gamemode setter"
assert 'utils::mayKeepGameMode(static_cast<int>(mode)' in reconcile
assert '"setGameMode", revoke_mode,' in reconcile
assert '"setNameTag", !has("nickname") && !has("nickname.other"),' in reconcile
assert '"revokeGod", !has("god") && !has("god.other"),' in reconcile
assert '"clearAfkCache", !has("afk"),' in reconcile
assert 'if (has(node)) {' in reconcile and 'node, found != intervals->end(),' in reconcile
for forbidden in ("setHealth(", "setMaxHealth(", "runTask", "sendPacket", "dispatchCommand", "catch ("):
    assert forbidden not in reconcile, f"Unexpected fix/timing/failure behavior introduced: {forbidden}"

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
    assert "utils::hudTestUsages()" in source, "Both registrations must use the tested public command syntax"
assert "hudSelectionList(plugin.hud_test.selectedMask())" in command
assert "hudSelectionList(plugin.hud_test.selectedMask(), false)" in command
assert "[HUDTest]" in plugin and "getHealth()" in plugin, "Diagnostics must include snapshots of server health"
workflow = read(".github/workflows/build.yml")
assert "branches: [chromevale-permissions-fix, chromevale-health-hud-test]" in workflow
print("HUD join-only component gates, both flight/synchronization sites, original setter ordering, failure/completion and console wiring checked.")

# Build 3 observes loading and repair timing without implementing a repair.
timeline = without_comments(read("src/utils/hud_timeline.cpp"))
assert 'EventPriority::Monitor, false' in plugin
assert 'registerEvent(&EventListener::onHudPacket,' in plugin
assert 'packet_id != 113 && packet_id != 312' in timeline
assert 'stage=before-native-handling' in timeline
assert 'event.isCancelled()' in timeline
assert 'event.getAddress()' in timeline and 'event.getSubClientId()' in timeline
assert 'decodeHudLoadingPacket(payload)' in timeline and 'hudPayloadHex(payload)' in timeline
assert 'hud_timeline.active(uuid, session)' in timeline
assert 'hud_timeline.generation() != generation' in timeline
assert 'const auto name = player ?' in timeline and 'const auto uuid = player ?' in timeline
callback = timeline.split('[this, uuid, name, session,', 1)[1].split('if (task)', 1)[0]
for forbidden in ('getPlayer(', 'player->', 'reloadCustomPerms(', 'reconcilePlayerState(', 'setWalkSpeed(', 'setFlySpeed('):
    assert forbidden not in callback, f'Log marker must not access/mutate player state: {forbidden}'
for forbidden in ('setPayload(', 'setCancelled(', 'sendMessage(', 'sendPacket(', 'dispatchCommand(', 'db->'):
    assert forbidden not in timeline, f'Packet observer must remain read-only: {forbidden}'
assert 'hud_timeline.start(player.getUniqueId().str())' in plugin
assert 'hud_timeline.end(event.getPlayer().getUniqueId().str())' in plugin
assert 'hud_timeline.clear()' in plugin and 'cancelTask(task)' in plugin
assert 'event=join.next-tick-sync' in join
assert 'event=reconcile.begin' in reconcile and 'event=reconcile.end' in reconcile
assert 'hudStamp(&player)' in sync and 'hudStamp(&player)' in reconcile
speed = without_comments(read('src/commands/movement/speed.cpp'))
assert speed.count('player->setWalkSpeed(') == 1 and speed.count('player->setFlySpeed(') == 1
for kind in ('Walk', 'Fly'):
    assert f'!request->query && (mode == Mode::{kind} || mode == Mode::Both)' in speed
    assert f'event=manual-speed.begin setter=set{kind}Speed' in speed
    assert f'event=manual-speed.end setter=set{kind}Speed' in speed
assert '3.4.3-chromevale.12-hudtest.3' in plugin
print('Loading observer, session-bound log markers, lifecycle timestamps and manual speed instrumentation checked.')
