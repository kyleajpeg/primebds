# ChromeVale .7

This supersedes .6's Owner restrictions and its installation guide where they conflict.

Owner and Operator are trusted saved roles. They bypass the hierarchy on targets,
rank assignment (including self and Owner/Operator), spy privacy, and broad/native
commands previously reserved for the panel. Ordinary ranks still fail closed.
Wildcard grants or a large weight do not turn another rank into a trusted role.
The existing command permission checks and enabled-command configuration remain.
After demoting yourself to an ordinary rank, recovery requires the panel or another
trusted administrator; your account does not retain a hidden personal bypass.

Warnings notify online recipients in chat and with a form offering their history.
Default gains `primebds.command.warnings.self`: `/warnings` or `/warnings "Your Name"`
reads only your own records and grants no delete/clear rights. Staff's existing
warnings node still requires lower-ranked targets; trusted administrators bypass.
The warnings database gets one additive `expires_at` column. Existing timestamps
remain intact, old expiry is marked unknown, and new warnings store permanent or
explicit expiry correctly. Dismissing the form does not delete the warning.

Speed now accepts `/speed <number> [player]`, mode-first values/resets and reset-first
forms. The mode enum appears once in command metadata. Target arguments use strings
to avoid native selector serialization; the plugin resolves them with hierarchy
checks. Quote gamertags containing spaces. Speed values retain their previous raw
units: normal walk is 0.1 and normal flight is 0.05, not multiplier 1.

On an actual saved-rank change, settings are reconciled against the new effective
permissions (including inheritance and per-player overrides). Still-authorized
settings remain unchanged. Unavailable spy/chat/AFK settings are reset; PM defaults
to enabled only if permission to toggle PM is lost. Unavailable god/flight/speed,
nickname, monitor/blockscan and game modes are reset. Game-mode capability checks
recognize native gamemode, gmc/gmt, gma and gmsp. Creative/spectator retain intrinsic
flight when their game mode remains authorized. Cleared toggles are never restored
automatically by promotion. Reassigning the same rank and ordinary permission reloads
do not reset gameplay state. Punishments, inventory, XP, homes and world data remain.

ModSpy includes `[ModSpy attempt]` player command attempts and `[ModSpy action]`
moderation results. An attempt is not evidence of success. Trusted roles see all
other players' attempts, including malformed/denied commands. Ordinary ModSpy viewers
must outrank every identified participant, including whisper/reply recipients.
Unknown/global/indirect commands, staff-channel messages and ambiguous selectors
are withheld from ordinary viewers. SocialSpy retains its own toggle and both-party
privacy checks. Moderator is not newly granted ModSpy.

The Linux CI suite exercises policy combinations, warning access, speed parsing,
spy recipient extraction, real SQLite setting persistence/migration, and existing
regressions. Live Endstone parsing, forms and client autocomplete still need an
acceptance check after replacement and reconnect.
