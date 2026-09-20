# ChromeVale .6: staff hierarchy

This revision adds shared rank checks to PrimeBDS target resolution, registered
command handlers and player command interception. Permission nodes still decide
which commands a player can run; rank weight additionally decides whom they can
target. A higher number means a higher rank. Equal weights are protected.

## Rules

- Other players must be strictly lower in rank. Utility commands can still target
  yourself; moderation and rank assignment cannot.
- Both participants must be strictly lower than the viewer for SocialSpy, including
  `/reply`. ModSpy similarly checks the action's actor and target. Current spy
  permissions and toggles are required every time a message is sent.
- Missing/invalid rank weights fail closed. Owner is protected from every in-game
  actor. Operator is additionally protected from non-Owners, even with bad weights.
- A selector is resolved once and the entire action is refused if it includes a
  protected player. There is no partial application to a mixed set.
- `primebds.command.rank.set` grants only `/rank set`. Both the current target and
  destination rank must be below the actor. Self-changes and delegated assignment
  of native-op or unrestricted rank/permission administration are refused.
- Full rank-definition editing requires Owner plus `primebds.command.rank` (or
  the panel console). The console is the recovery path and can assign Owner.
  An in-game Owner can assign lower Operator, but cannot assign Owner or alter
  another Owner. Existing wildcard grants do not override these checks.
- Rank-definition edits refresh the saved permission snapshot, caches and all
  online permission attachments immediately, including inherited permissions.
  `/primebds reloadconfig` also reloads ranks and refreshes online players.
- Warning/note deletion verifies record ownership before deleting by ID.
- `/unmute` clears both normal and silent mute; `/silentmute` remains a toggle.
  Silent mute remains in-memory and does not survive server restart.

## Broad commands and native syntax

IP bans/mutes, global mute, allowlist changes, indirect native execution
(`/execute`, `/function`, scripts and scheduling), native reload/config commands,
scoreboards, unknown commands and native multi-target selectors require the panel.
Reviewed native target commands accept literal player names and usually `@s`;
native teleport is limited to `/tp <player> <player>`. PrimeBDS utility commands
retain their selector support with the shared rank checks.

World/server management is Owner/panel-only. This is a direct command-target and
spy policy, not a sandbox against PvP, block changes, command blocks, add-ons,
another plugin's server-side actions or trusted console access. Newly installed
plugins need their own review before delegation. Broad Owner configuration remains
trusted administration; Owner can edit rank definitions.

## Moderator deployment configuration

The server-specific upload package adds Moderator at weight 25, inheriting Default.
It grants `minecraft.command.kick` and PrimeBDS mute, tempmute, silentmute, unmute,
warn, unwarn, warnings, clearchat, punishments, staffchat and socialspy. Native op is
explicitly false. No rank.set, ban, modspy or creative utility grants are included.
No account is assigned Moderator automatically. JSON preserves literal § symbols.

`/clearchat` clears the actor and lower-ranked clients only. Staff chat is a shared
staff channel, so staff recipients can see higher staff's messages there; it is
not SocialSpy or private-message interception.

## Validation and limits

CTest covers target/assignment ordering, all combinations of spy participants,
protected roles, malformed quoted commands, command classification, selector/spy
guard wiring and the existing regression suites. Compilation runs on Linux CI,
not through local Windows executables. These checks do not replace a live Endstone
acceptance test of native command parsing, permission visibility and event delivery.

Use a maintenance session with an Owner, two Moderators and two Default testers.
Verify lower-target moderation succeeds and peer/higher attempts have no effect.
Test `/tell`, `/w`, `/msg` and `/reply` in both directions with SocialSpy enabled;
only Default-to-Default traffic should reach Moderator. Verify permission removal
and inherited changes take effect without reconnecting. Use a temporary intermediate
rank to test delegated rank.set and restore it afterward. Confirm restart persistence.
