#pragma once

#include "primebds/utils/external_permissions.h"
#include <endstone/permissions/permission_attachment.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace primebds::utils {

// Ownership must not depend on effective permission markers: a later attachment
// can shadow them. Removal callbacks also handle removals initiated by Endstone.
class OwnedPermissionAttachments {
    using Attachments = std::vector<endstone::PermissionAttachment *>;
    using State = std::map<std::string, Attachments>;
    std::shared_ptr<State> state_ = std::make_shared<State>();
public:
    void track(const std::string &key, endstone::PermissionAttachment *attachment) {
        (*state_)[key].push_back(attachment);
        attachment->setRemovalCallback([state = std::weak_ptr<State>(state_), key](const auto &removed) {
            if (const auto locked = state.lock()) {
                const auto it = locked->find(key);
                if (it == locked->end()) return;
                std::erase(it->second, &removed);
                if (it->second.empty()) locked->erase(it);
            }
        });
    }
    void clear(const std::string &key) {
        const auto found = state_->find(key);
        if (found == state_->end()) return;
        auto attachments = std::move(found->second);
        state_->erase(found);
        for (auto *attachment : attachments) {
            attachment->setRemovalCallback({});
            attachment->remove();
        }
    }
    // For a new login/session or shutdown after players are already destroyed.
    // Never dereference pointers from a previous player lifetime.
    void forget(const std::string &key) { state_->erase(key); }
    void forgetAll() { state_->clear(); }
    std::size_t size(const std::string &key) const {
        const auto found = state_->find(key);
        return found == state_->end() ? 0 : found->second.size();
    }
};

// The caller has already established legacy permissions and native OP state.
// Every touched descendant receives a later exact pin, including protected
// descendants: unordered iteration inside Endstone cannot decide the result.
template <typename Player, typename Plugin, typename Track>
bool applyExternalPermissionLayers(Player &player, Plugin &plugin,
                                   const external::Analysis &analysis,
                                   const external::Values &desired, Track track) {
    std::set<std::string> seeds;
    for (const auto &[node, value] : desired)
        if (!analysis.unsafe_nodes.contains(node)) seeds.insert(node);
    const auto touched = external::closure(analysis, seeds);
    external::Values pins = desired;
    for (const auto &node : touched)
        if (external::isProtected(node)) pins[node] = player.hasPermission(node);

    const auto apply = [&](const std::vector<std::string> &nodes) {
        endstone::PermissionAttachment *attachment = nullptr;
        for (const auto &node : nodes) {
            if (!touched.contains(node) || !pins.contains(node)) continue;
            if (!attachment) {
                attachment = player.addAttachment(plugin);
                if (!attachment) return false;
                track(attachment);
            }
            attachment->setPermission(node, pins.at(node));
        }
        return true;
    };
    for (const auto &nodes : analysis.layers)
        if (!apply(nodes)) return false;

    // Configured exact permissions need not be registered. They are terminal
    // nodes until a future restart/reload discovers a real permission definition.
    std::vector<std::string> terminals;
    for (const auto &[node, value] : desired)
        if (!analysis.graph.contains(node) && !analysis.unsafe_nodes.contains(node))
            terminals.push_back(node);
    return apply(terminals);
}

} // namespace primebds::utils
