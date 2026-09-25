"""Fail CI when a new command/target path lacks an explicit hierarchy decision."""
from pathlib import Path
import re
import sys
root = Path(sys.argv[1])
metadata = (root / 'src/commands/command_metadata.cpp').read_text(encoding='utf-8')
policy = (root / 'include/primebds/utils/hierarchy_policy.h').read_text(encoding='utf-8')
commands = set(re.findall(r'cmd\(b, "([^"]+)"\)', metadata))
classified = set()
for names in re.findall(r'add\(CommandPolicy::\w+, "([^"]+)"\)', policy):
    classified.update(names.split())
classified.update(re.findall(r'p\.emplace\("([^"]+)"', policy))
assert commands == classified, (commands-classified, classified-commands)
for path in (root/'src').rglob('*.cpp'):
    source=path.read_text(encoding='utf-8')
    assert 'getMatchingActors(plugin.getServer(),' not in source, path
    if path.name != 'target_selector.cpp':
        assert 'getMatchingActorsRaw(' not in source, path
for name in ('unwarn.cpp','warnings.cpp'):
    source=(root/'src/commands/moderation'/name).read_text(encoding='utf-8')
    assert 'Warning ID does not belong to this player.' in source
assert 'Note ID does not belong to this player.' in (root/'src/commands/message/note.cpp').read_text(encoding='utf-8')
for path in ('src/commands/message/reply.cpp','src/handlers/preprocesses/command_intercept.cpp'):
    source=(root/path).read_text(encoding='utf-8')
    assert 'hierarchy::socialSpy(' in source
    assert 'enabled_ss' not in source, 'Spy fanout must stay centralized'
print(f'All {len(commands)} commands classified; selector, record and spy guard wiring checked.')

# Personal warning help must never advertise a target or destructive action.
self_help = metadata.split('cmd(b, "warnings")', 1)[1].split('cmd(b, "staffwarnings")', 1)[0]
assert '/warnings [page: int]' in self_help
assert '<player:' not in self_help and 'delete' not in self_help and 'clear' not in self_help
staff_help = metadata.split('cmd(b, "staffwarnings")', 1)[1].split('// ---', 1)[0]
assert '.permissions("primebds.command.warnings")' in staff_help
assert 'primebds.command.warnings.self' not in staff_help
rank = (root/'src/commands/server/rank.cpp').read_text(encoding='utf-8')
assert 'hierarchy::rankOf(user->internal_rank), destination' in rank
assert 'assignRank(user->xuid, key)' in rank
assert 'not found online' not in rank
print('Personal warning help, staff permission, and offline rank guard wiring checked.')

for node in ('set', 'list', 'info'):
    permission = 'primebds.command.rank.' + node
    assert permission in metadata and permission in rank
    block = metadata.split(f'cmd(b, "rank{node}")',1)[1].split('cmd(b,',1)[0]
    assert f'.permissions("{permission}")' in block
    assert 'dispatchCommand' not in rank, 'Delegation must not run as console'
root_rank = metadata.split('cmd(b, "rank")',1)[1].split('cmd(b,',1)[0]
assert '.permissions("primebds.command.rank")' in root_rank
assert 'primebds.command.rank.' not in root_rank, 'Delegated grants must not expose the complete rank grammar'
root_registration = rank.split('REGISTER_COMMAND(rank,',1)[1].split('static bool executeRankAction',1)[0]
assert 'info.permissions = {"primebds.command.rank"}' in root_registration
assert 'primebds.command.rank.' not in root_registration
assert 'isCommandEnabled("rank" + action)' in rank and 'isCommandEnabled("rank")' in rank
assert 'player.updateCommands()' in (root/'src/plugin.cpp').read_text(encoding='utf-8')
assert 'mayUseRankAction(' in rank
chat = (root/'src/handlers/chat.cpp').read_text(encoding='utf-8')
cords = (root/'src/commands/misc/cords.cpp').read_text(encoding='utf-8')
assert 'handlers::sendPublicChat(plugin, *player,' in cords
public = chat.split('bool sendPublicChat(',1)[1]
assert public.index('callEvent(event)') < public.index('if (event.isCancelled()) return false;') < public.index('recipient->sendMessage(rendered)')
assert '&event != plugin.public_chat_event' in chat
for path in ('src/handlers/chat.cpp', 'src/commands/message/voice.cpp'):
    assert 'hierarchy::isGloballyMuted(' in (root/path).read_text(encoding='utf-8')
for path in ('src/handlers/chat.cpp','src/commands/message/staffchat.cpp'):
    assert 'utils::staffChatMessage(' in (root/path).read_text(encoding='utf-8')
print('Delegated rank nodes, coordinate chat event, scoped mute and shared staff formatting checked.')

for name in ('spawn','warp','home','homeother','back','offlinetp','top','bottom'):
    source = (root/f'src/commands/movement/{name}.cpp').read_text(encoding='utf-8')
    assert 'performCommand(' not in source and 'dispatchCommand(' not in source, name
    assert re.search(r'if \(!utils::teleport(?:Saved|Logout|Here)\([^\n]+\)\) return true;',source), name
    if name in ('spawn','warp','home','back'):
        guard = source.index('if (!utils::teleport')
        commit = source.index(f'{name}_cooldowns[player->getXuid()] = now;')
        assert guard < commit, f'{name} cooldown committed before teleport success'
adapter = (root/'src/utils/teleport.cpp').read_text(encoding='utf-8')
assert 'return player.teleport(endstone::Location(dimension,' in adapter
assert 'target.pitch.value_or(current.getPitch())' in adapter and 'target.yaw.value_or(current.getYaw())' in adapter
offline = (root/'src/commands/movement/offlinetp.cpp').read_text(encoding='utf-8')
assert 'user->last_logout_pos, user->last_logout_dim' in offline
assert '/offlinetp <player: string>' in metadata and '/offlinetp <player: string>' in offline
mute = (root/'src/commands/moderation/globalmute.cpp').read_text(encoding='utf-8')
assert 'mayManageGlobalMute(' in mute and 'mayLiftGlobalMute(' not in mute
assert 'globalMuteExempt(player.hasPermission("primebds.command.globalmute")' in (root/'src/utils/hierarchy.cpp').read_text(encoding='utf-8')
for path in ('src/commands/server/rank.cpp','src/commands/server/filterlist.cpp'):
    assert 'utils::sortRanks(ranks)' in (root/path).read_text(encoding='utf-8')
print('Full-rank visibility, independent delegation, teleport success gates and permission-based mute exemption checked.')

# .12 command visibility, native-list interception and deferred notices.
lookup = metadata.split('cmd(b, "playerrank")',1)[1].split('cmd(b,',1)[0]
assert '.permissions("primebds.command.playerrank")' in lookup
lookup_source = (root/'src/commands/server/playerrank.cpp').read_text(encoding='utf-8')
assert 'getUniqueUserByName(args[0])' in lookup_source
assert 'getMatchingActors' not in lookup_source and 'dispatchCommand' not in lookup_source
assert 'info.permissions = {"primebds.command.playerrank"}' in lookup_source
intercept = (root/'src/handlers/preprocesses/command_intercept.cpp').read_text(encoding='utf-8')
assert intercept.count('if (cmd == "list" && args.size() == 1)') == 2
assert intercept.index('canInterceptPlayerCommand(') < intercept.index('utils::sendRankedPlayerList(plugin, player)')
assert '{"list", "minecraft.command.list"}' in (root/'include/primebds/handlers/preprocesses/command_authorization.h').read_text(encoding='utf-8')
plugin = (root/'src/plugin.cpp').read_text(encoding='utf-8').split('bool PrimeBDS::reloadCustomPerms(',1)[1]
assert plugin.index('if (!attachment)') < plugin.index('reconcilePlayerState(player)') < plugin.index('pendingRankNotice(') < plugin.index('clearPendingRankNotice(')
assert 'Your rank is now' in plugin
assert 'assignRank(player.getXuid(), "Default")' in (root/'src/utils/permissions/permission_manager.cpp').read_text(encoding='utf-8')
assert '(lower ranks only)' not in mute
print('Rank lookup registration, list authorization, and deferred notification wiring checked.')

# .14: peer targeting is limited to native teleport; all permission writes use the serializer.
hierarchy_source = (root/'src/utils/hierarchy.cpp').read_text(encoding='utf-8')
teleport_branch = hierarchy_source.split('else if (name == "teleport" || name == "tp")',1)[1].split('} else if',1)[0]
assert 'for (const auto &target : *targets)' in teleport_branch
assert 'canTeleportTarget(actor_rank, playerRank(plugin, target))' in teleport_branch
assert hierarchy_source.count('canTeleportTarget(')==1, 'Peer policy must not leak to other actions'
assert 'return requireTarget(plugin, sender, target, allow_self);' in hierarchy_source
assert '"effect"' in hierarchy_source.split('static const std::set<std::string> first_target',1)[1].split(';',1)[0]
assert 'bool authorizeNativeCommand(' in hierarchy_source
assert '{"teleport", "minecraft.command.teleport"}, {"tp", "minecraft.command.teleport"}' in (root/'include/primebds/handlers/preprocesses/command_authorization.h').read_text(encoding='utf-8')
config_source=(root/'src/utils/config/config_manager.cpp').read_text(encoding='utf-8')
permission_writes=config_source.split('nlohmann::json ConfigManager::loadPermissions()',1)[1].split('std::vector<std::string> ConfigManager::loadRules()',1)[0]
assert permission_writes.count('writeTextFile(')==4 and permission_writes.count('utils::serializePermissions(')==4
assert '.dump(' not in permission_writes, 'No permissions write may bypass ordered serialization'
print('Native-only peer teleport policy, unchanged effect authorization and centralized permissions serialization checked.')
