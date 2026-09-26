"""Verify the thin Endstone adapters retain dispatch, audit and admission invariants."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
source = (root / 'src/handlers/preprocesses/command_intercept.cpp').read_text(encoding='utf-8')
adapter = source.split('static CommandRouting resolveCommandRouting(', 1)[1].split('static bool denyCommandRoute(', 1)[0]
assert 'getPluginCommand(commandLookupName(label))' in adapter
assert '&owner == &plugin' in adapter and 'command->getName()' in adapter
assert 'command->isRegistered()' in adapter and 'owner.isEnabled()' in adapter
assert 'command->getPermissions()' in adapter and 'externalPermissionGraph().unsafe_nodes' in adapter
assert 'unsafe.contains(hierarchy::lower(node))' in adapter
assert 'CommandRegistry::instance().find(name)' in adapter
assert 'canonicalName' not in adapter, 'Do not drop namespaces before actual registration lookup'

player = source.split('void handleCommandPreprocess(', 1)[1].split('void handleServerCommandPreprocess(', 1)[0]
console = source.split('void handleServerCommandPreprocess(', 1)[1]
for handler in (player, console):
    assert handler.index('if (event.isCancelled())') < handler.index('resolveCommandRouting(')
    assert handler.index('denyCommandRoute(') < handler.index('if (routing.route == CommandRoute::External) return;')
    assert handler.index('if (routing.route == CommandRoute::External) return;') < handler.index('if (PARSE_COMMANDS.find(cmd)')
    prefix = handler.split('if (routing.route == CommandRoute::External) return;', 1)[0]
    for forbidden in ('dispatchCommand(', 'performCommand(', 'setOp(', 'event.setCommand('):
        assert forbidden not in prefix, 'External commands must retain original text and sender'
    assert 'routing.policy_name.empty() ? hierarchy::canonicalName(args[0]) : routing.policy_name' in handler
    assert 'resolveCommandRouting(plugin, commandLabel(command))' in handler
    assert handler.index('resolveCommandRouting(') < handler.index('splitCommand(')
assert 'if (routing.route == CommandRoute::Existing)' in player.split('args = splitCommand(', 1)[0]
assert console.index('CommandRoute::External') < console.index('splitCommand(')
assert player.index('CommandRoute::External') < player.index('hierarchy::authorizePluginCommand(')
assert 'hierarchy::authorizeNativeCommand(plugin, player, cmd, arguments)' in player
assert player.index('if (!authorized)') < player.index('canInterceptPlayerCommand(')

plugin = (root / 'src/plugin.cpp').read_text(encoding='utf-8')
entry = plugin.split('void EventListener::onPlayerCommand(', 1)[1].split('void EventListener::onServerCommand(', 1)[0]
assert entry.index('permissions_pending.contains(') < entry.index('handleCommandPreprocess(')
assert 'event.setCancelled(true)' in entry
assert 'EventPriority::Lowest, false' in plugin and 'utils::formatCommandAttempt(' in plugin
print('External dispatch adapter preserves native authorization, original sender, audit and admission guards.')
