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
