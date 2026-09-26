#include "primebds/utils/external_permission_attachments.h"
#include "endstone/core/permissions/permissible_base.h"
#include "endstone/core/server.h"
#include "endstone/plugin/plugin_loader.h"
#include <entt/locator/locator.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>

// The test links Endstone's unchanged permissible_base.cpp. Only the server
// locator, permission registry, and plugin lifecycle are supplied here; the
// recursive child expansion and attachment storage are the real implementation.
namespace ext = primebds::utils::external;

namespace {
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

class TestPlugin final : public endstone::Plugin {
public:
    TestPlugin() { setEnabled(true); }
    const endstone::PluginDescription &getDescription() const override { return description_; }
private:
    endstone::PluginDescription description_ =
        endstone::detail::PluginDescriptionBuilder{}.build("attachment-test", "1.0.0");
};

class TestPluginManager final : public endstone::PluginManager {
public:
    void registerLoader(std::unique_ptr<endstone::PluginLoader>) override {}
    endstone::Plugin *getPlugin(const std::string &) const override { return nullptr; }
    std::vector<endstone::Plugin *> getPlugins() const override { return {}; }
    bool isPluginEnabled(const std::string &) const override { return false; }
    bool isPluginEnabled(endstone::Plugin *plugin) const override { return plugin && plugin->isEnabled(); }
    endstone::Plugin *loadPlugin(std::string) override { return nullptr; }
    std::vector<endstone::Plugin *> loadPlugins(std::string) override { return {}; }
    std::vector<endstone::Plugin *> loadPlugins(std::vector<std::string>) override { return {}; }
    void enablePlugin(endstone::Plugin &) const override {}
    void enablePlugins() const override {}
    void disablePlugin(endstone::Plugin &) override {}
    void disablePlugins() override {}
    void clearPlugins() override {}
    void callEvent(endstone::Event &) override {}
    void registerEvent(std::string, std::function<void(endstone::Event &)>,
                       endstone::EventPriority, endstone::Plugin &, bool) override {}
    endstone::Permission *getPermission(std::string name) const override {
        const auto it = permissions_.find(name);
        return it == permissions_.end() ? nullptr : it->second.get();
    }
    endstone::Permission &addPermission(std::unique_ptr<endstone::Permission> permission) override {
        permission->init(*this);
        auto &result = *permission;
        permissions_[permission->getName()] = std::move(permission);
        return result;
    }
    void removePermission(endstone::Permission &permission) override { removePermission(permission.getName()); }
    void removePermission(std::string name) override { permissions_.erase(name); }
    std::vector<endstone::Permission *> getDefaultPermissions(endstone::PermissionLevel level) const override {
        std::vector<endstone::Permission *> result;
        for (const auto &[name, permission] : permissions_) {
            const auto value = permission->getDefault();
            const bool granted = value == endstone::PermissionDefault::True ||
                (value == endstone::PermissionDefault::Operator && level >= endstone::PermissionLevel::Operator) ||
                (value == endstone::PermissionDefault::NotOperator && level == endstone::PermissionLevel::Default) ||
                (value == endstone::PermissionDefault::Console && level == endstone::PermissionLevel::Console);
            if (granted) result.push_back(permission.get());
        }
        return result;
    }
    void recalculatePermissionDefaults(endstone::Permission &) override {}
    void subscribeToPermission(std::string, endstone::Permissible &) override {}
    void unsubscribeFromPermission(std::string, endstone::Permissible &) override {}
    std::unordered_set<endstone::Permissible *> getPermissionSubscriptions(std::string) const override { return {}; }
    void subscribeToDefaultPerms(endstone::PermissionLevel, endstone::Permissible &) override {}
    void unsubscribeFromDefaultPerms(endstone::PermissionLevel, endstone::Permissible &) override {}
    std::unordered_set<endstone::Permissible *> getDefaultPermSubscriptions(endstone::PermissionLevel) const override {
        return {};
    }
    std::unordered_set<endstone::Permission *> getPermissions() const override {
        std::unordered_set<endstone::Permission *> result;
        for (const auto &[name, permission] : permissions_) result.insert(permission.get());
        return result;
    }
    void registerGraph(const ext::Graph &graph, bool reverse_children = false) {
        for (const auto &[name, children] : graph) {
            std::unordered_map<std::string, bool> actual_children;
            if (reverse_children) {
                for (auto it = children.rbegin(); it != children.rend(); ++it) actual_children.insert(*it);
            } else {
                for (const auto &child : children) actual_children.insert(child);
            }
            addPermission(std::make_unique<endstone::Permission>(
                name, "", endstone::PermissionDefault::False, std::move(actual_children)));
        }
    }
private:
    std::unordered_map<std::string, std::unique_ptr<endstone::Permission>> permissions_;
};

struct Environment {
    TestPluginManager manager;
    TestPlugin plugin;
    Environment() {
        endstone::core::EndstoneServer::plugin_manager = &manager;
        entt::locator<endstone::core::EndstoneServer>::available = true;
    }
    ~Environment() {
        entt::locator<endstone::core::EndstoneServer>::available = false;
        endstone::core::EndstoneServer::plugin_manager = nullptr;
    }
};

void expectDesired(endstone::Permissible &player, const ext::Values &desired) {
    for (const auto &[name, expected] : desired) {
        if (player.hasPermission(name) != expected) {
            throw std::runtime_error("Endstone effective permission does not match resolved value: " + name);
        }
    }
}

void worldEditAndRevocation() {
    Environment environment;
    const auto analysis = ext::analyze({
        {"worldedit", {{"worldedit.command.pos1", true}, {"worldedit.command.pos2", true},
                       {"worldedit.command.set", true}, {"worldedit.command.undo", true}}}
    });
    environment.manager.registerGraph(analysis.graph);
    endstone::core::PermissibleBase player(nullptr);
    auto *other = player.addAttachment(environment.plugin, "other-plugin.unmanaged", true);
    check(other != nullptr, "Unrelated plugin attachment could not be created");
    primebds::utils::OwnedPermissionAttachments owned;
    const auto track = [&](auto *attachment) { owned.track("player-session", attachment); };

    ext::Layer rank;
    rank.explicit_nodes = {{"worldedit", true}, {"worldedit.command.undo", false}};
    const auto desired = ext::resolve(analysis, rank);
    check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, desired, track),
          "WorldEdit attachment application failed");
    expectDesired(player, desired);
    check(player.hasPermission("worldedit.command.pos1") && player.hasPermission("worldedit.command.pos2"),
          "WorldEdit event-handler permission checks must see the same grants as command checks");
    check(!player.hasPermission("worldedit.command.undo"), "Group expansion erased an explicit command denial");
    check(player.getPermissionLevel() == endstone::PermissionLevel::Default,
          "External permissions must not require operator status");

    check(owned.size("player-session") >= 2, "Ancestors and descendants need separate owned attachment layers");
    owned.clear("player-session");
    check(owned.size("player-session") == 0, "Owned attachment state was not cleared");
    check(player.hasPermission("other-plugin.unmanaged"), "Refresh removed another plugin's attachment");
    const auto revoked = ext::resolve(analysis, {});
    check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, revoked, track),
          "Revoked attachment application failed");
    expectDesired(player, revoked);
    check(!player.hasPermission("worldedit.command.set"), "Revocation left a stale command grant");
    check(player.removeAttachment(*other), "Other plugin's attachment was no longer owned by the player");

    // Endstone or another plugin can initiate removal. Its callback must remove
    // the pointer from our ownership registry so the next refresh is safe.
    endstone::PermissionAttachment *removed = nullptr;
    for (auto *info : player.getEffectivePermissions()) {
        if (info->getPermission() == "worldedit.command.set") removed = info->getAttachment();
    }
    check(removed != nullptr, "No effective attachment available to exercise external removal");
    const auto previous_count = owned.size("player-session");
    check(player.removeAttachment(*removed), "Framework-initiated removal failed");
    check(owned.size("player-session") + 1 == previous_count, "Removal callback left a dangling owned pointer");
    owned.clear("player-session");
    check(owned.size("player-session") == 0, "Refresh after external removal was not safe");
}

void invertedChildren() {
    Environment environment;
    const auto analysis = ext::analyze({{"switch.group", {{"switch.ordinary", true}, {"switch.inverted", false}}}});
    environment.manager.registerGraph(analysis.graph);
    for (int mode = 0; mode < 3; ++mode) {
        endstone::core::PermissibleBase player(nullptr);
        ext::Layer rank;
        if (mode != 0) rank.explicit_nodes["switch.group"] = mode == 2;
        const auto desired = ext::resolve(analysis, rank);
        check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, desired, [](auto *) {}),
              "Inverted-child attachment application failed");
        expectDesired(player, desired);
        check(player.hasPermission("switch.inverted") == (mode == 1),
              "Only an explicit false group may derive an inverted child grant");
        check(player.hasPermission("switch.ordinary") == (mode == 2), "Ordinary child expansion is incorrect");
    }
}

void overlappingGroupsAndProtectedPins() {
    for (int variant = 0; variant < 12; ++variant) {
        Environment environment;
        const auto analysis = ext::analyze({
            {"edit.allow", {{"edit.shared", true}, {"edit.deep", true}}},
            {"edit.deny", {{"edit.shared", true}}},
            {"edit.shared", {{"edit.leaf", true}}},
            {"edit.deep", {{"edit.shared", true}}},
            {"edit.escalate", {{"minecraft.command.op", true}, {"primebdsoverride", false}}},
            {"minecraft.command.op", {{"edit.protected-descendant", true}}}
        });
        environment.manager.registerGraph(analysis.graph, variant % 2 != 0);
        endstone::core::PermissibleBase player(nullptr);
        check(player.addAttachment(environment.plugin, "minecraft.command.op", false), "Legacy deny setup failed");
        check(player.addAttachment(environment.plugin, "primebdsoverride", true), "Legacy marker setup failed");
        ext::Layer rank;
        rank.explicit_nodes = {{"edit.allow", true}, {"edit.deny", false}, {"edit.escalate", true}};
        const auto desired = ext::resolve(analysis, rank);
        check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, desired, [](auto *) {}),
              "Overlapping graph attachment application failed");
        expectDesired(player, desired);
        check(!player.hasPermission("edit.shared") && !player.hasPermission("edit.leaf"),
              "Conflicting derived permissions must remain denied after real Endstone expansion");
        check(!player.hasPermission("minecraft.command.op"), "External permission group changed a protected permission");
        check(player.hasPermission("primebdsoverride"), "External permission group erased the legacy attachment marker");
        for (int repeat = 0; repeat < 5; ++repeat) {
            player.recalculatePermissions();
            expectDesired(player, desired);
            check(!player.hasPermission("minecraft.command.op") && player.hasPermission("primebdsoverride"),
                  "Framework recalculation changed protected permission pins");
        }
    }
}

void terminalAndDefaultNodes() {
    Environment environment;
    // The child's permission is deliberately unregistered, as supported by
    // Endstone's child definitions. The resolver must still pin the terminal.
    const ext::Graph registered = {{"tools.group", {{"tools.unregistered", true}}}, {"tools.public", {}}};
    const auto analysis = ext::analyze(registered);
    environment.manager.registerGraph(registered);
    environment.manager.getPermission("tools.public")->setDefault(endstone::PermissionDefault::True);
    endstone::core::PermissibleBase player(nullptr);
    player.recalculatePermissions();
    check(player.hasPermission("tools.public"), "Test did not exercise a real default-true permission");
    ext::Layer rank;
    rank.explicit_nodes = {{"tools.unregistered", true}, {"tools.exact-extra", true}};
    const auto desired = ext::resolve(analysis, rank);
    check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, desired, [](auto *) {}),
          "Unregistered terminal attachment application failed");
    expectDesired(player, desired);
    check(player.hasPermission("tools.unregistered"), "Unregistered child terminal grant was lost");
    check(player.hasPermission("tools.exact-extra"), "Exact undeclared node grant was lost");
    check(!player.hasPermission("tools.public"), "Managed external default-true permission bypassed rank policy");
}

void unsafeCyclesAreNeverAttached() {
    Environment environment;
    const auto analysis = ext::analyze({
        {"unsafe.parent", {{"unsafe.a", true}}},
        {"unsafe.a", {{"unsafe.b", true}}},
        {"unsafe.b", {{"unsafe.a", false}}},
        {"safe.command", {}}
    });
    environment.manager.registerGraph(analysis.graph);
    endstone::core::PermissibleBase player(nullptr);
    ext::Layer rank;
    rank.wildcard = true;
    const auto desired = ext::resolve(analysis, rank);
    check(primebds::utils::applyExternalPermissionLayers(player, environment.plugin, analysis, desired, [](auto *) {}),
          "Safe part of cyclic graph could not be materialized");
    check(player.hasPermission("safe.command"), "Unrelated safe permissions were discarded with a cyclic group");
    for (const auto &node : analysis.unsafe_nodes) {
        check(!player.isPermissionSet(node), "Unsafe cycle or ancestor was attached and could recurse forever");
        check(!player.hasPermission(node), "Unsafe permission unexpectedly remained granted");
    }
}

void partialFailureAndPlayerLifetime() {
    Environment environment;
    const auto analysis = ext::analyze({{"failure.group", {{"failure.command", true}}}});
    environment.manager.registerGraph(analysis.graph);
    primebds::utils::OwnedPermissionAttachments owned;
    {
        endstone::core::PermissibleBase player(nullptr);
        auto *unrelated = player.addAttachment(environment.plugin, "unrelated.keep", true);
        check(unrelated != nullptr, "Failure-case unrelated attachment setup failed");
        struct FailingPlayer {
            endstone::core::PermissibleBase &base;
            unsigned allocations_left = 1;
            bool hasPermission(const std::string &node) const { return base.hasPermission(node); }
            endstone::PermissionAttachment *addAttachment(endstone::Plugin &plugin) {
                if (allocations_left == 0) return nullptr;
                --allocations_left;
                return base.addAttachment(plugin);
            }
        } failing{player};
        ext::Layer rank;
        rank.explicit_nodes["failure.group"] = true;
        const auto desired = ext::resolve(analysis, rank);
        check(!primebds::utils::applyExternalPermissionLayers(failing, environment.plugin, analysis, desired,
                    [&](auto *attachment) { owned.track("same-uuid", attachment); }),
              "Allocation failure must propagate to the caller instead of accepting a partial refresh");
        check(owned.size("same-uuid") == 1, "Partially allocated attachment was not tracked for cleanup");
        owned.clear("same-uuid");
        check(!player.hasPermission("failure.command"), "Partial grant survived failure cleanup");
        check(player.hasPermission("unrelated.keep"), "Failure cleanup removed an unrelated attachment");

        auto *stale_after_destruction = player.addAttachment(environment.plugin, "session.old", true);
        check(stale_after_destruction != nullptr, "Old session setup failed");
        owned.track("same-uuid", stale_after_destruction);
    }
    // Endstone destroys its attachment objects with the player without calling
    // removeAttachment. The login/session boundary must forget old pointers.
    owned.forget("same-uuid");
    check(owned.size("same-uuid") == 0, "New login retained attachments from a destroyed player");
    {
        endstone::core::PermissibleBase replacement(nullptr);
        auto *current = replacement.addAttachment(environment.plugin, "session.current", true);
        check(current != nullptr, "New session setup failed");
        owned.track("same-uuid", current);
        owned.clear("same-uuid");
        check(!replacement.hasPermission("session.current"), "New session did not own its replacement attachment");
        auto *shutdown = replacement.addAttachment(environment.plugin, "session.shutdown", true);
        check(shutdown != nullptr, "Shutdown setup failed");
        owned.track("same-uuid", shutdown);
    }
    owned.forgetAll();
    check(owned.size("same-uuid") == 0, "Shutdown retained destroyed-player pointers");
}
} // namespace

int main() {
    try {
        worldEditAndRevocation();
        invertedChildren();
        overlappingGroupsAndProtectedPins();
        terminalAndDefaultNodes();
        unsafeCyclesAreNeverAttached();
        partialFailureAndPlayerLifetime();
        std::cout << "Real Endstone attachment expansion regression tests passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
