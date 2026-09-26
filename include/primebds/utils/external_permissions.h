#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// External permission policy is independent of Endstone so the same resolved
// snapshot can drive permission checks, ordered attachments and regression tests.
namespace primebds::utils::external {

using Values = std::map<std::string, bool>;
using Graph = std::map<std::string, Values>;

inline std::string normalize(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

inline bool isProtected(const std::string &name) {
    const auto normalized = normalize(name);
    const auto prefix = normalized.substr(0, normalized.find('.'));
    return prefix == "minecraft" || prefix == "endstone" || prefix == "primebds" ||
           prefix == "primebdsoverride";
}

inline bool isExternal(const std::string &name) {
    return !name.empty() && name != "*" && !isProtected(name);
}

struct Layer {
    Values explicit_nodes;
    std::optional<bool> wildcard;
};

inline Layer layerFromValues(const Values &values) {
    Layer result;
    for (const auto &[raw, value] : values) {
        const auto name = normalize(raw);
        if (name == "*") result.wildcard = value;
        else if (isExternal(name)) result.explicit_nodes[name] = value;
    }
    return result;
}

// Call in the existing parent-first rank traversal order. A broader declaration
// never deletes an inherited exact exception; only the same exact key replaces it.
inline void mergeLayer(Layer &target, const Layer &source) {
    if (source.wildcard) target.wildcard = source.wildcard;
    for (const auto &[name, value] : source.explicit_nodes)
        if (isExternal(name)) target.explicit_nodes[normalize(name)] = value;
}

struct Analysis {
    Graph graph;
    std::set<std::string> cycle_nodes;
    // Includes every ancestor whose attachment would recurse into a cycle.
    std::set<std::string> unsafe_nodes;
    // Safe actual graph, ordered parents before descendants. Filtering these
    // levels to a closed materialization set retains deterministic ordering.
    std::vector<std::vector<std::string>> layers;
    std::vector<std::pair<std::string, std::string>> protected_edges;
};

inline Analysis analyze(const Graph &input) {
    Analysis result;
    for (const auto &[raw_parent, children] : input) {
        const auto parent = normalize(raw_parent);
        if (parent.empty() || parent == "*") continue;
        auto &target = result.graph[parent];
        for (const auto &[raw_child, value] : children) {
            const auto child = normalize(raw_child);
            if (!child.empty() && child != "*") target[child] = value;
        }
    }
    std::set<std::string> terminals;
    std::map<std::string, std::set<std::string>> parents;
    for (const auto &[parent, children] : result.graph) {
        for (const auto &[child, value] : children) {
            terminals.insert(child);
            parents[child].insert(parent);
            if (isExternal(parent) && isProtected(child))
                result.protected_edges.emplace_back(parent, child);
        }
    }
    for (const auto &name : terminals) result.graph.try_emplace(name);

    // Tarjan identifies actual cyclic components; a reverse walk below also
    // excludes ancestors, even when the edge into a cycle crosses policy scopes.
    std::map<std::string, int> index, low;
    std::vector<std::string> stack;
    std::set<std::string> on_stack;
    int next = 0;
    std::function<void(const std::string &)> visit = [&](const std::string &node) {
        index[node] = low[node] = next++;
        stack.push_back(node);
        on_stack.insert(node);
        for (const auto &[child, value] : result.graph.at(node)) {
            if (!index.contains(child)) {
                visit(child);
                low[node] = std::min(low[node], low[child]);
            } else if (on_stack.contains(child)) {
                low[node] = std::min(low[node], index[child]);
            }
        }
        if (low[node] != index[node]) return;
        std::vector<std::string> component;
        while (!stack.empty()) {
            auto child = stack.back();
            stack.pop_back();
            on_stack.erase(child);
            component.push_back(child);
            if (child == node) break;
        }
        if (component.size() > 1 || result.graph.at(node).contains(node))
            result.cycle_nodes.insert(component.begin(), component.end());
    };
    for (const auto &[node, children] : result.graph)
        if (!index.contains(node)) visit(node);

    std::vector<std::string> pending(result.cycle_nodes.begin(), result.cycle_nodes.end());
    result.unsafe_nodes = result.cycle_nodes;
    for (std::size_t i = 0; i < pending.size(); ++i)
        for (const auto &parent : parents[pending[i]])
            if (result.unsafe_nodes.insert(parent).second) pending.push_back(parent);

    std::map<std::string, std::size_t> indegree;
    for (const auto &[node, children] : result.graph)
        if (!result.unsafe_nodes.contains(node)) indegree[node] = 0;
    for (const auto &[node, children] : result.graph) {
        if (!indegree.contains(node)) continue;
        for (const auto &[child, value] : children)
            if (indegree.contains(child)) ++indegree[child];
    }
    std::set<std::string> ready;
    for (const auto &[node, count] : indegree)
        if (count == 0) ready.insert(node);
    while (!ready.empty()) {
        result.layers.emplace_back(ready.begin(), ready.end());
        std::set<std::string> next_level;
        for (const auto &node : ready) {
            for (const auto &[child, value] : result.graph.at(node)) {
                auto found = indegree.find(child);
                if (found != indegree.end() && --found->second == 0) next_level.insert(child);
            }
        }
        ready = std::move(next_level);
    }
    return result;
}

// Real Endstone traversal closure, including protected nodes. Unsafe seeds are
// omitted: callers must reject/report them, never attach even a false value.
inline std::set<std::string> closure(const Analysis &analysis, const std::set<std::string> &seeds) {
    std::set<std::string> result;
    std::vector<std::string> pending;
    for (const auto &raw : seeds) {
        const auto node = normalize(raw);
        if (node.empty() || node == "*" || analysis.unsafe_nodes.contains(node)) continue;
        if (result.insert(node).second) pending.push_back(node);
    }
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto found = analysis.graph.find(pending[i]);
        if (found == analysis.graph.end()) continue;
        for (const auto &[child, value] : found->second)
            if (!analysis.unsafe_nodes.contains(child) && result.insert(child).second)
                pending.push_back(child);
    }
    return result;
}

namespace detail {
inline Layer normalizedLayer(const Layer &layer) {
    auto result = layerFromValues(layer.explicit_nodes);
    if (layer.wildcard) result.wildcard = layer.wildcard;
    return result;
}

// Returns only active exact/derived decisions. An absent or wildcard-only parent
// must not invert a false child edge into an unintended grant.
inline Values activeValues(const Analysis &analysis, const Layer &layer) {
    Values result;
    std::map<std::string, bool> proposals;
    for (const auto &level : analysis.layers) {
        for (const auto &node : level) {
            if (!isExternal(node)) continue;
            std::optional<bool> value;
            if (const auto exact = layer.explicit_nodes.find(node); exact != layer.explicit_nodes.end())
                value = exact->second;
            else if (const auto derived = proposals.find(node); derived != proposals.end())
                value = derived->second;
            if (!value) continue;
            result[node] = *value;
            for (const auto &[child, edge] : analysis.graph.at(node)) {
                if (!isExternal(child) || analysis.unsafe_nodes.contains(child)) continue;
                const bool child_value = edge == *value;
                const auto [it, inserted] = proposals.emplace(child, child_value);
                if (!inserted) it->second = it->second && child_value; // Equal-tier conflicts deny.
            }
        }
    }
    // Configured exact nodes may be intentionally unregistered permission strings.
    for (const auto &[node, value] : layer.explicit_nodes)
        if (!analysis.unsafe_nodes.contains(node)) result[node] = value;
    return result;
}
} // namespace detail

inline Values resolve(const Analysis &analysis, const Layer &rank_input,
                      const Layer &user_input = {}, const std::set<std::string> &extra_nodes = {}) {
    const auto rank = detail::normalizedLayer(rank_input);
    const auto user = detail::normalizedLayer(user_input);
    Values result;
    const bool baseline = rank.wildcard.value_or(false);
    for (const auto &[node, children] : analysis.graph)
        if (isExternal(node)) result[node] = baseline;
    for (const auto &raw : extra_nodes) {
        const auto node = normalize(raw);
        if (isExternal(node)) result[node] = baseline;
    }
    for (const auto &[node, value] : rank.explicit_nodes) result.try_emplace(node, baseline);
    for (const auto &[node, value] : user.explicit_nodes) result.try_emplace(node, baseline);
    for (const auto &[node, value] : detail::activeValues(analysis, rank)) result[node] = value;
    if (user.wildcard)
        for (auto &[node, value] : result) value = *user.wildcard;
    for (const auto &[node, value] : detail::activeValues(analysis, user)) result[node] = value;
    for (const auto &node : analysis.unsafe_nodes)
        if (isExternal(node)) result[node] = false;
    return result;
}

} // namespace primebds::utils::external
