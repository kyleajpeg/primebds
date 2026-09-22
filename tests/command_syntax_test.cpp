#include "primebds/utils/admin_commands.h"
#include "primebds/utils/warning_command.h"
#include "primebds/utils/rank_tools.h"
#include "endstone/core/command/command_usage_parser.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
using endstone::core::CommandUsageParser;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
int main() {
    try {
        int rank_forms = 0;
        for (const auto &usage : primebds::utils::filterListUsages()) {
            const auto parsed = CommandUsageParser(usage).parse();
            check(parsed.has_value(), "Endstone rejected rank directory usage");
            const auto &params = parsed->parameters;
            if (!params.empty() && params[0].values == std::vector<std::string>{"rank"}) {
                check(params.size() == 3 && params[1].type == "string" && !params[1].optional &&
                      params[2].type == "int" && params[2].optional, "Rank names must support current and future ranks with optional pagination");
                ++rank_forms;
            }
        }
        check(rank_forms == 1, "Rank membership syntax missing");
        // Exercise Endstone's actual parser, not only our handler's argument parser.
        int numeric_modes = 0, reset_forms = 0;
        std::set<std::string> mode_values;
        for (const auto &usage : primebds::utils::speedUsages()) {
            auto parsed = CommandUsageParser(usage).parse();
            check(parsed.has_value(), "Endstone rejected speed usage");
            const auto &p = parsed->parameters;
            check(!p.empty(), "Missing speed arguments");
            if (p[0].is_enum && std::find(p[0].values.begin(),p[0].values.end(),"walkspeed") != p[0].values.end()) {
                check(p.size() == 3 && p[1].type == "float" && p[1].name == "multiplier" && !p[1].optional, "Numeric mode value must be a required Bedrock float, not an identifier");
                check(p[2].optional && p[2].type == "string", "Target remains optional");
                for (const auto &value : p[0].values) check(mode_values.insert(value).second, "Duplicate mode autocomplete entries");
                ++numeric_modes;
            }
            if (p[0].is_enum && p[0].values == std::vector<std::string>{"reset"}) ++reset_forms;
        }
        check(numeric_modes == 1 && reset_forms == 1, "Both numeric and reset branches must be registered");
        int finite = 0, permanent = 0;
        for (const auto &usage : primebds::utils::warningUsages()) {
            auto parsed = CommandUsageParser(usage).parse();
            check(parsed.has_value(), "Endstone rejected warning usage");
            const auto &p = parsed->parameters;
            for (const auto &arg : p) check(!arg.optional, "Warning duration and reason cannot be optional");
            check(p.back().type == "message", "Free text reason must be last and capture spaces");
            if (p[1].type == "int") {
                check(p.size() == 4 && p[2].is_enum && p[2].values.size() == 14, "Duration units must be advertised as an enum");
                ++finite;
            } else {
                check(p.size() == 3 && p[1].is_enum && p[1].values == std::vector<std::string>{"permanent"}, "Permanent requires an explicit keyword");
                ++permanent;
            }
        }
        check(finite == 1 && permanent == 1, "Both warning duration branches are registered");
        std::cout << "Endstone usage parser regression tests passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
