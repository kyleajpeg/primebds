#include "primebds/utils/external_permissions.h"
#include "primebds/utils/external_rank_layer.h"
#include "primebds/utils/permission_snapshot.h"

#include <cstdlib>
#include <iostream>

namespace ext = primebds::utils::external;

static void check(bool condition, const std::string &description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(EXIT_FAILURE);
    }
}

static ext::Layer layer(ext::Values values) { return ext::layerFromValues(values); }

int main() {
    const auto worldedit = ext::analyze({
        {"worldedit", {{"worldedit.command.set", true}, {"worldedit.command.copy", true},
                       {"worldedit.command.pos1", true}}},
        {"worldedit.command.set", {}}, {"worldedit.command.copy", {}},
        {"worldedit.command.pos1", {}}
    });
    auto values = ext::resolve(worldedit, {});
    check(values.size() == 4 && !values.at("worldedit") && !values.at("worldedit.command.set"),
          "all managed external nodes default deny without deriving implicit root false");
    values = ext::resolve(worldedit, layer({{"worldedit.command.set", true}}));
    check(!values.at("worldedit") && values.at("worldedit.command.set") && !values.at("worldedit.command.copy"),
          "a command leaf grant works independently of its implicit denied group");
    values = ext::resolve(worldedit, layer({{"worldedit", true}, {"worldedit.command.set", false}}));
    check(values.at("worldedit") && !values.at("worldedit.command.set") && values.at("worldedit.command.copy"),
          "explicit leaf denial survives an explicit broad group grant");

    auto inherited = layer({{"worldedit.command.set", false}});
    ext::mergeLayer(inherited, layer({{"worldedit", true}}));
    values = ext::resolve(worldedit, inherited);
    check(!values.at("worldedit.command.set") && values.at("worldedit.command.copy"),
          "descendant group grant preserves inherited exact exception");
    ext::mergeLayer(inherited, layer({{"*", true}}));
    check(!ext::resolve(worldedit, inherited).at("worldedit.command.set"),
          "descendant wildcard is a baseline and preserves inherited exact exception");
    ext::mergeLayer(inherited, layer({{"worldedit.command.set", true}}));
    check(ext::resolve(worldedit, inherited).at("worldedit.command.set"),
          "descendant declaration of the same exact node overrides its inherited value");

    const auto rank = layer({{"worldedit", true}, {"worldedit.command.set", false}});
    values = ext::resolve(worldedit, rank, layer({{"worldedit", true}}));
    check(values.at("worldedit.command.set"), "user-derived group grant outranks rank exact denial");
    values = ext::resolve(worldedit, rank, layer({{"worldedit", true}, {"worldedit.command.set", false}}));
    check(!values.at("worldedit.command.set"), "user exact denial outranks user-derived group grant");
    values = ext::resolve(worldedit, rank, layer({{"*", false}, {"worldedit.command.copy", true}}));
    check(!values.at("worldedit") && !values.at("worldedit.command.set") && values.at("worldedit.command.copy"),
          "user wildcard replaces rank tier but retains user exact exceptions");

    auto rank_json = nlohmann::json::parse(R"({
        "dEfAuLt": {"permissions": ["Public.Use", "minecraft.command.help"]},
        "Base": {"inherits": ["default"], "permissions": {"worldedit.command.set": false, "worldedit.command.copy": true}},
        "Branch": {"inherits": ["DEFAULT"], "permissions": {"worldedit.command.copy": false}},
        "Admin": {"inherits": ["Base", "Branch"], "permissions": {"worldedit": true}},
        "Co-Owner": {"inherits": ["ADMIN"], "permissions": {"WORLDEDIT.command.SET": true}},
        "Array": {"permissions": ["WorldEdit", "primebds.minecraft.op"]},
        "WildcardChild": {"inherits": ["Admin"], "permissions": {"*": true}},
        "LoopA": {"inherits": ["LoopB"], "permissions": {"loop.a": true}},
        "LoopB": {"inherits": ["LoopA"], "permissions": {"loop.b": false}}
    })");
    auto json_layer = ext::collectRankLayer(rank_json, "aDmIn");
    values = ext::resolve(worldedit, json_layer);
    check(values.at("worldedit") && !values.at("worldedit.command.set") &&
          !values.at("worldedit.command.copy") && values.at("worldedit.command.pos1") && values.at("public.use"),
          "actual JSON traversal preserves inherited exact exceptions and ordered multi-parent overrides");
    values = ext::resolve(worldedit, ext::collectRankLayer(rank_json, "co-owner"));
    check(values.at("worldedit.command.set") && !values.at("worldedit.command.copy"),
          "JSON exact-node child override is case insensitive and leaves other exceptions intact");
    const auto fallback = ext::collectRankLayer(rank_json, "MissingRank");
    check(fallback.explicit_nodes == ext::Values{{"public.use", true}} && !fallback.wildcard,
          "unknown ranks use case-insensitive Default and filter protected permission declarations");
    json_layer = ext::collectRankLayer(rank_json, "Array");
    check(json_layer.explicit_nodes == ext::Values{{"worldedit", true}},
          "legacy permission arrays are exact external grants without protected authority");
    json_layer = ext::collectRankLayer(rank_json, "WildcardChild");
    check(json_layer.wildcard == true && !ext::resolve(worldedit, json_layer).at("worldedit.command.set"),
          "JSON child wildcard stays separate from inherited exact denial");
    check(ext::collectRankLayer(rank_json, "LoopA").explicit_nodes == ext::Values{{"loop.a", true}, {"loop.b", false}},
          "rank inheritance cycles terminate with the existing first-visit traversal behavior");
    rank_json["Branch"]["permissions"]["worldedit.command.set"] = true;
    check(ext::resolve(worldedit, ext::collectRankLayer(rank_json, "Admin")).at("worldedit.command.set"),
          "same-node declaration from later inherited rank replaces the earlier declaration");
    check(ext::collectRankLayer(nlohmann::json::object(), "Missing").explicit_nodes.empty(),
          "missing Default produces no external grants");

    const auto inverse = ext::analyze({{"toggle", {{"toggle.enabled", true}, {"toggle.inverse", false}}}});
    check(!ext::resolve(inverse, {}).at("toggle.inverse"), "implicit false root must never grant an inverted child");
    values = ext::resolve(inverse, layer({{"toggle", false}}));
    check(!values.at("toggle.enabled") && values.at("toggle.inverse"), "explicit false group honors inverted child edges");
    values = ext::resolve(inverse, layer({{"*", true}}));
    check(values.at("toggle") && values.at("toggle.enabled") && values.at("toggle.inverse"),
          "wildcard grants every managed node without accidental graph-derived denial");
    values = ext::resolve(inverse, layer({{"*", true}, {"toggle", true}}));
    check(!values.at("toggle.inverse"), "active explicit group decision overrides wildcard baseline");

    const auto diamond = ext::analyze({
        {"parent.a", {{"child", true}}}, {"parent.b", {{"child", false}}},
        {"child", {{"grandchild", true}}}
    });
    values = ext::resolve(diamond, layer({{"parent.a", true}, {"parent.b", true}}));
    check(!values.at("child") && !values.at("grandchild"), "conflicting derived paths deny and propagate the resolved denial");
    values = ext::resolve(diamond, layer({{"parent.a", true}, {"parent.b", true}, {"child", true}}));
    check(values.at("child") && values.at("grandchild"), "explicit intermediate exception governs its own descendants");

    const auto protected_graph = ext::analyze({
        {"tools", {{"minecraft.command", true}, {"tools.safe", true}, {"primebds.minecraft.op", true}}},
        {"minecraft.command", {{"minecraft.command.op", true}, {"bridge.external", true}}}
    });
    values = ext::resolve(protected_graph, layer({{"tools", true}, {"minecraft.command", true},
                                               {"primebds.minecraft.op", true}}));
    check(!values.contains("minecraft.command") && !values.contains("primebds.minecraft.op") &&
          values.at("tools.safe") && !values.at("bridge.external"),
          "external derivation stops at protected authority and cannot bridge through it");
    const auto touched = ext::closure(protected_graph, {"tools", "unregistered.exact"});
    check(touched.contains("minecraft.command.op") && touched.contains("bridge.external") &&
          touched.contains("primebds.minecraft.op") && touched.contains("unregistered.exact"),
          "materialization uses actual transitive graph including protected and unknown terminals");
    check(protected_graph.protected_edges.size() == 2, "protected edge diagnostics identify only cross-scope incoming edges");
    std::map<std::string, std::size_t> depth;
    for (std::size_t i = 0; i < protected_graph.layers.size(); ++i)
        for (const auto &node : protected_graph.layers[i]) depth[node] = i;
    for (const auto &[parent, children] : protected_graph.graph)
        for (const auto &[child, edge] : children)
            check(depth.at(parent) < depth.at(child), "attachment levels order every actual parent before descendants");

    const auto cyclic = ext::analyze({
        {"ancestor", {{"cycle.a", true}}}, {"cycle.a", {{"cycle.b", true}}},
        {"cycle.b", {{"cycle.a", true}, {"leaf", true}}}, {"self", {{"self", false}}},
        {"independent", {{"leaf", true}}}
    });
    check(cyclic.cycle_nodes == std::set<std::string>({"cycle.a", "cycle.b", "self"}) &&
          cyclic.unsafe_nodes.contains("ancestor") && !cyclic.unsafe_nodes.contains("leaf"),
          "cycles and recursion-reaching ancestors are unsafe, but independent leaves remain usable");
    values = ext::resolve(cyclic, layer({{"*", true}, {"ancestor", true}, {"independent", true}}));
    check(!values.at("ancestor") && !values.at("cycle.a") && !values.at("self") && values.at("leaf"),
          "wildcard cannot grant unsafe nodes; independent acyclic permissions still resolve");
    check(ext::closure(cyclic, {"ancestor", "independent"}) == std::set<std::string>({"independent", "leaf"}),
          "unsafe seeds are excluded even when they would have a false attachment value");
    for (const auto &level : cyclic.layers)
        for (const auto &node : level) check(!cyclic.unsafe_nodes.contains(node), "unsafe nodes never enter attachment levels");

    const auto normalized = ext::analyze({{"Mixed.Root", {{"Different.Node", true}}}});
    values = ext::resolve(normalized, layer({{"MIXED.ROOT", true}, {"Unregistered.Custom", true}}));
    check(values.at("different.node") && values.at("unregistered.custom"),
          "names normalize and arbitrary registered child relationships cross external namespaces");
    values = ext::resolve(normalized, layer({{"mixed.*", true}}));
    check(!values.at("mixed.root") && !values.at("different.node") && values.at("mixed.*"),
          "unregistered dotted wildcard has no invented prefix expansion");
    check(ext::isProtected("PRIMEBDS.minecraft.op") && ext::isProtected("primebdsoverride.external") &&
          ext::isExternal("minecraftish.tool"), "protected scopes have exact namespace boundaries");

    ext::Values legacy{{"primebds.command", true}, {"primebds.command.speed", false},
                       {"primebds.minecraft.op", false}, {"minecraft.command.op", false},
                       {"worldedit", true}, {"worldedit.command.set", false}};
    primebds::utils::applyPluginPermissionGroups(legacy);
    check(legacy.at("primebds.command.speed") && !legacy.at("primebds.minecraft.op") &&
          !legacy.at("minecraft.command.op") && !legacy.at("worldedit.command.set"),
          "legacy normalization preserves core behavior while leaving external exact exceptions intact");
    std::cout << "External permission graph, precedence, scope and cycle regressions passed.\n";
}
