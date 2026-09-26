#include <endstone/command/plugin_command.h>
#include <endstone/permissions/permission.h>

#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>

static void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// The sender supplies effective permissions; the command gate itself is the
// actual Endstone PluginCommand implementation used by normal server dispatch.
class TestSender final : public endstone::CommandSender {
public:
    std::set<std::string> grants;
    mutable int errors = 0;
    mutable int messages = 0;

    endstone::PermissionLevel getPermissionLevel() const override { return endstone::PermissionLevel::Default; }
    bool isPermissionSet(std::string name) const override { return grants.contains(name); }
    bool isPermissionSet(const endstone::Permission &permission) const override {
        return isPermissionSet(permission.getName());
    }
    bool hasPermission(std::string name) const override { return grants.contains(name); }
    bool hasPermission(const endstone::Permission &permission) const override {
        return hasPermission(permission.getName());
    }
    endstone::PermissionAttachment *addAttachment(endstone::Plugin &, const std::string &, bool) override {
        throw std::runtime_error("Command execution must not add permission attachments");
    }
    endstone::PermissionAttachment *addAttachment(endstone::Plugin &) override {
        throw std::runtime_error("Command execution must not add permission attachments");
    }
    bool removeAttachment(endstone::PermissionAttachment &) override { return false; }
    void recalculatePermissions() override {}
    std::unordered_set<endstone::PermissionAttachmentInfo *> getEffectivePermissions() const override { return {}; }
    endstone::ConsoleCommandSender *asConsole() const override { return nullptr; }
    endstone::BlockCommandSender *asBlock() const override { return nullptr; }
    endstone::Actor *asActor() const override { return nullptr; }
    endstone::Player *asPlayer() const override { return nullptr; }
    void sendMessage(const endstone::Message &) const override { ++messages; }
    void sendErrorMessage(const endstone::Message &) const override { ++errors; }
    endstone::Server &getServer() const override { throw std::runtime_error("No server is needed for this gate test"); }
    std::string getName() const override { return "NonOpTestSender"; }
};

class TestPlugin final : public endstone::Plugin {
public:
    endstone::CommandSender *last_sender = nullptr;
    int calls = 0;
    int effects = 0;
    std::vector<std::string> last_args;

    const endstone::PluginDescription &getDescription() const override { return description_; }
    void enable(bool enabled) { setEnabled(enabled); }
    bool onCommand(endstone::CommandSender &sender, const endstone::Command &,
                   const std::vector<std::string> &args) override {
        last_sender = &sender;
        last_args = args;
        ++calls;
        if (!args.empty() && args.front() == "restricted" && !sender.hasPermission("example.restricted")) {
            return false;
        }
        ++effects;
        return true;
    }

private:
    endstone::PluginDescription description_{"routing_test", "1.0.0"};
};

int main() {
    TestPlugin plugin;
    plugin.enable(true);
    TestSender sender;
    const endstone::Command description("paint", "", {"/paint"}, {"brush"},
                                         {"example.paint", "example.alternate"});
    endstone::PluginCommand command(description, plugin);
    check(&command.getPlugin() == &plugin, "Command retains actual plugin ownership");

    (void)command.execute(sender, {"stone"});
    check(plugin.calls == 0 && sender.errors == 1, "No grant stops the executor at Endstone's real gate");
    sender.grants.insert("example.paint");
    check(command.execute(sender, {"stone", "quoted value"}), "A declared permission permits dispatch");
    check(plugin.calls == 1 && plugin.effects == 1 && plugin.last_sender == &sender &&
              plugin.last_args == std::vector<std::string>{"stone", "quoted value"},
          "The original non-op sender and parsed arguments reach the plugin unchanged");
    check(sender.getPermissionLevel() == endstone::PermissionLevel::Default,
          "Plugin command access does not change operator status");

    check(!command.execute(sender, {"restricted"}) && plugin.calls == 2 && plugin.effects == 1,
          "An internal subcommand check remains able to deny a declared-command grant");
    sender.grants.insert("example.restricted");
    check(command.execute(sender, {"restricted"}) && plugin.effects == 2,
          "The plugin's independent internal permission controls its restricted action");

    sender.grants = {"example.alternate"};
    check(command.execute(sender, {}) && plugin.effects == 3,
          "Multiple declared command permissions retain Endstone's any-of semantics");
    sender.grants.clear();
    (void)command.execute(sender, {});
    check(plugin.calls == 4 && plugin.effects == 3 && sender.errors == 2,
          "Revocation immediately blocks subsequent command execution");

    plugin.enable(false);
    sender.grants.insert("example.paint");
    check(!command.execute(sender, {}) && plugin.calls == 4 && sender.messages == 1,
          "A disabled plugin cannot execute even with a grant");
    plugin.enable(true);
    endstone::PluginCommand public_command(endstone::Command("publicinfo"), plugin);
    sender.grants.clear();
    check(public_command.execute(sender, {}) && plugin.effects == 4,
          "A plugin command declaring no permissions keeps the author's public-command semantics");
    std::cout << "Actual Endstone plugin permission gate, sender preservation and executor checks passed.\n";
}
