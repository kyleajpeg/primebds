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
