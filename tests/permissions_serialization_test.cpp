#include "primebds/utils/permissions_serialization.h"
#include "primebds/utils/rank_tools.h"
#include <iostream>
#include <stdexcept>
using nlohmann::json;
using primebds::utils::serializePermissions;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
std::vector<std::string> keys(const std::string &text) {
    const auto parsed=nlohmann::ordered_json::parse(text);
    std::vector<std::string> result;
    for (const auto &[name, value] : parsed.items()) result.push_back(name);
    return result;
}
int main() {
    try {
        json data={
            {"Owner",{{"weight",1000},{"permissions",{{"*",true},{"minecraft.command.op",true}}}}},
            {"Operator",{{"weight",100}}}, {"Co-Owner",{{"weight",75}}},
            {"Admin",{{"weight",50},{"inherits",json::array({"Moderator","Builder"})},
                {"permissions",{{"z.last",false},{"a.first",true}}},
                {"prefix","§8[§4Admin§8] §4"},{"suffix","§r: "},{"custom",{{"nested",json::array({3,1,2})}}}}},
            {"Moderator",{{"weight",25}}}, {"Default",{{"weight",0}}}};
        const auto text=serializePermissions(data);
        check(keys(text)==std::vector<std::string>{"Default","Moderator","Admin","Co-Owner","Operator","Owner"},"ascending normal ranks");
        check(json::parse(text)==data,"save preserves complete JSON data, including custom fields and arrays");
        check(text.find("§8[§4Admin§8] §4")!=std::string::npos && text.find("\\u00a7")==std::string::npos,"literal section signs preserved");
        check(text.starts_with("{\n    \"Default\": {\n        \"weight\": 0"),"four-space indentation retained");
        check(text.find("\"a.first\"")<text.find("\"z.last\""),"nested object keys remain alphabetical");
        check(serializePermissions(json::parse(text))==text,"repeated saves are stable");
        data["Admin"]["weight"]=-10;
        check(keys(serializePermissions(data)).front()=="Admin","edited weight used immediately, without cached ranks");
        check(serializePermissions(json::object())=="{}","empty permissions object");
        json edge={
            {"alpha",{{"weight",5}}},{"Alpha",{{"weight",5}}},{"Beta",{{"weight",5}}},
            {"Negative",{{"weight",-4}}},{"Zero",{{"weight",0}}},
            {"Maximum",{{"weight",std::numeric_limits<std::int64_t>::max()}}},
            {"Minimum",{{"weight",std::numeric_limits<std::int64_t>::min()}}},
            {"InvalidBoolean",{{"weight",true}}},{"InvalidFloat",{{"weight",1.5}}},
            {"InvalidIntegerFloat",{{"weight",2.0}}},{"InvalidNull",{{"weight",nullptr}}},
            {"InvalidMissing",json::object()},{"InvalidString",{{"weight","3"}}},
            {"InvalidOverflow",{{"weight",std::numeric_limits<std::uint64_t>::max()}}},
            {"InvalidRecord",json::array({1,2})}};
        const auto edgeText=serializePermissions(edge);
        check(keys(edgeText)==std::vector<std::string>{"Minimum","Negative","Zero","Alpha","alpha","Beta","Maximum",
            "InvalidBoolean","InvalidFloat","InvalidIntegerFloat","InvalidMissing","InvalidNull","InvalidOverflow","InvalidRecord","InvalidString"},
            "numeric order, deterministic alphabetical ties and invalid weights last");
        check(json::parse(edgeText)==edge,"invalid weight data is retained rather than repaired or discarded");
        check(serializePermissions(json::parse(edgeText))==edgeText,"invalid records serialize stably");
        std::vector<primebds::hierarchy::Rank> display{{"Default",0},{"Owner",1000},{"Admin",50}};
        primebds::utils::sortRanks(display);
        check(display.front().name=="Owner" && display.back().name=="Default","in-game rank lists remain descending");
        std::cout << "Permissions serialization regression tests passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
