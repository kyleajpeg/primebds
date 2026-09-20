# ChromeVale permission fix

Based on upstream PrimeBDS v3.4.3, commit 4979fe2ae044828c834433655c83f2c1b177fb11.
The plugin identifies itself as **3.4.3-chromevale.2** and keeps the `primebds`
plugin name and existing data directory. Rank JSON, player databases, world
files and chat colors are not replaced by this patch.

## Changes

- Version `.2` adds a console `[CommandAudit]` entry for every player command
  event received, before PrimeBDS checks permissions or remaps commands.
  Denied, unknown and already-cancelled submissions are included. Each entry
  records an attempt, not successful execution. Existing native logs remain,
  so commands reaching normal dispatch can also have a native log entry.
- The audit listener uses Lowest priority with ignore_cancelled=false and
  never changes the event. Another plugin at the same priority can still
  modify text before this listener; this is an event log, not packet capture.
- The console audit includes command arguments (including whispers), uses no
  new webhook or database, and escapes control/formatting characters to keep
  records on one readable line. Ordinary chat keeps its existing logging.
  Unsent typing and commands rejected by the client never reach this listener.
  Logging costs one additional line and linear text formatting per event;
  high command spam will increase log volume. No performance benchmark is claimed.

- Authorize every intercepted player command before privileged side effects.
- Require both `minecraft.command.op`/`minecraft.command.deop` and
  `primebds.command.rank` for those aliases, because they change a saved rank.
- Route op/deop rank changes through ordinary command dispatch, preserving
  the rank command's permission check and commands.json enable switch.
- Remove unconditional setOp calls after rank dispatch: only a successful
  rank application should synchronize operator status.
- Check permissions again in the plugin command executor and rank handler.
- Respect command events already cancelled by another plugin.

The panel console remains the recovery/admin interface. `/rank set <player>
Owner` sets the Owner rank; `/op <player>` continues to select Operator.
Grant rank-management permission only to users trusted to assign any rank.

## Build in this fork

Push the `chromevale-permissions-fix` branch. The **ChromeVale Linux build**
workflow builds on Ubuntu 22.04 with Clang 18, runs the C++ authorization
regression tests, and checks that the binary requires no newer than GLIBC 2.35.
It uploads `primebds-chromevale-linux-<commit>` with:

- `endstone_primebds.so`
- `SHA256SUMS`
- `BUILD-INFO.txt` linking the binary to the source commit and workflow run

The workflow uses only read access to repository contents and does not publish
a release, deploy the server or delete past workflow runs. On a new fork,
GitHub may require the owner to enable Actions before the workflow can run.

## What the tests prove

The automated tests exercise the actual authorization helper used by the
interceptor: member denial, authorized staff delegation, op/deop's two required
nodes, alias equivalence, permission revocation, and fail-closed handling of
unknown interception targets. A successful build verifies compatibility with
the configured Endstone headers; it is not a live BDS permission test or a
complete audit of every PrimeBDS feature.

## Acceptance test before normal play

Keep the original vulnerable binary disabled. Back up the stopped server,
especially the world, root permissions.json and plugins/primebds databases.
Use a test instance/world or a maintenance session with only trusted testers.

1. Stop the server and replace only `plugins/endstone_primebds.so` with the
   new artifact. Do not install two copies. Preserve the existing data folder.
2. Start and verify **3.4.3-chromevale.2** loads without errors.
3. Using the panel, assign a connected test account Default. Check native op
   status as well. Clear any unintended per-player grants before the test.
4. From that player's game client, try self-op, op of another tester, deop,
   rank changes, kick of a trusted tester, and stop. All must be denied with
   no rank/op changes, disconnects or shutdown. Test aliases/namespaced forms
   where the server accepts them. A logged command attempt is not proof that
   it executed: inspect the actual account state and effects.
5. Verify ordinary chat, whispers and survival play still work. Reconnect and
   repeat the denial checks. Restart and repeat to verify persistence.
6. From the panel set the owner back to Owner; verify rank commands and normal
   administration work. Demote to Default and verify denial immediately.
7. In a test session, set the rank command disabled in commands.json and
   restart. Player op/deop aliases must not bypass the disabled command or
   grant native op. Restore the setting while stopped afterward.

If a check fails, stop and move the patched binary out of plugins. Keep the
test log and build provenance. Do not restore the vulnerable upstream binary
for normal multiplayer use; run Endstone without PrimeBDS while investigating.

No chat-format changes are included; message text remains white as requested.

## Command audit acceptance checks

As Default, submit `/op <your name>`, `/rank list`, and an unknown command.
Every submission that reaches Endstone's PlayerCommandEvent should produce a
`[CommandAudit]` line, even when denied or unrecognized. Self-op must still fail.
Check allowed commands as Owner, aliases, quoted arguments, and private messages.
Confirm ordinary chat still logs as before. Automated audit tests verify text
preservation and escaping of forged newlines, terminal escapes and color codes;
actual console output and event delivery require this live server check.
