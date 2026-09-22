#include "primebds/utils/rank_tools.h"
#include <iostream>
#include <stdexcept>
using namespace primebds;
static void check(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
int main() {
    try {
        const hierarchy::Rank owner{"Owner",1000}, op{"Operator",100}, co{"Co-Owner",75},
                              admin{"Admin",50}, mod{"Moderator",25}, member{"Default",0};
        std::set<std::string> grants{"primebds.command.rank.list"};
        auto has = [&](const std::string &node) { return grants.contains(node); };
        for (const auto *allowed : {"set","list","info"}) {
            auto single = [&](const std::string &node) { return node == "primebds.command.rank." + std::string(allowed); };
            for (const auto *action : {"set","list","info","create","delete","perm"})
                check(utils::mayUseRankAction(false,action,single)==(std::string(action)==allowed), "Each delegated rank node must work independently without sibling grants");
        }
        check(utils::mayUseRankAction(false,"list",has), "List-only delegation must work");
        for (auto action : {"set","info","create","delete","perm","inherit","weight","prefix","suffix",""})
            check(!utils::mayUseRankAction(false,action,has), "Read delegation leaked another subcommand");
        grants.insert("primebds.command.rank.info");
        grants.insert("primebds.command.rank.set");
        check(utils::mayUseRankAction(false,"info",has) && utils::mayUseRankAction(false,"set",has), "Independent delegated nodes");
        check(!utils::mayUseRankAction(false,"delete",has), "Delegated rank editing must be denied");
        for (auto name : {"motd","setrules","sethomes","setspawn","warps"})
            check(hierarchy::commandPolicy(name) == hierarchy::CommandPolicy::Shared, "Requested shared setting remains blocked");
        for (auto name : {"primebds","reloadscripts","updatepacks"})
            check(hierarchy::commandPolicy(name) == hierarchy::CommandPolicy::Owner, "Shared delegation leaked plugin/code management");
        check(hierarchy::commandPolicy("permissions") == hierarchy::CommandPolicy::Permissions, "Shared delegation leaked per-player permission edits");
        check(utils::mayUseRankAction(true,"delete",has), "Owner full management retained");
        check(hierarchy::canAssign(co,member,admin,false,false), "Co-Owner may appoint Admin");
        check(!hierarchy::canAssign(admin,member,admin,false,false), "Admin cannot appoint a peer");
        check(!hierarchy::canAssign(admin,co,mod,false,false), "Admin cannot demote Co-Owner");
        check(utils::globalMuteAffects(admin,admin,mod), "Admin mutes lower staff");
        for (auto target : {admin,co,op,owner})
            check(!utils::globalMuteAffects(admin,admin,target), "Global mute affected a protected rank");
        check(!utils::globalMuteAffects(admin,co,admin), "Promotion broadened existing mute");
        check(!utils::globalMuteAffects(co,admin,admin), "Demotion did not narrow existing mute");
        check(!utils::globalMuteAffects(admin,admin,{"Missing",std::nullopt}), "Unknown ranks are protected");
        check(utils::mayManageGlobalMute(false,true), "Permission holders can lift any issuer's mute, including peers, Owner or console");
        check(!utils::mayManageGlobalMute(false,false), "Players without the grant cannot toggle global mute");
        check(utils::mayManageGlobalMute(true,false), "Console retains control");
        check(utils::globalMuteExempt(true,false,false), "Global mute grant allows chat regardless of issuer");
        check(utils::globalMuteExempt(false,true,false) && utils::globalMuteExempt(false,false,true), "Existing exemptions retained");
        check(!utils::globalMuteExempt(false,false,false), "Ordinary players remain subject to scope");
        std::vector<hierarchy::Rank> ordered{member,admin,owner,mod,co,op,{"zUnknown",{}},{"aUnknown",{}},{"Builder",25},{"alpha",25}};
        utils::sortRanks(ordered);
        std::vector<std::string> names;
        for (const auto &rank : ordered) names.push_back(rank.name);
        check(names == std::vector<std::string>{"Owner","Operator","Co-Owner","Admin","alpha","Builder","Moderator","Default","aUnknown","zUnknown"}, "Descending numeric weights, case-insensitive ties, unknowns last");
        auto first = utils::rankPage(21,1), second = utils::rankPage(21,2), last = utils::rankPage(21,3);
        check(first && first->begin==0 && first->end==10 && first->pages==3, "First sorted page");
        check(second && second->begin==10 && second->end==20 && last && last->begin==20 && last->end==21, "Later sorted pages without missing or duplicate entries");
        check(!utils::rankPage(21,4) && !utils::rankPage(21,0) && !utils::rankPage(21,2147483647), "Invalid/overflow pages rejected");
        check(utils::rankPage(0,1)->end==0 && !utils::rankPage(0,2), "Empty rank list pagination");
        struct User { std::string xuid, name, internal_rank; };
        const std::vector<User> users{{"1","Alice","Admin"},{"2","Bob","admin"},
                                     {"3","Offline","FutureRank"},{"1","Duplicate","Admin"}};
        auto directory = utils::rankDirectory(std::vector<std::string>{"Default","Admin","Owner"},users);
        check(directory.at("admin").members == std::vector<std::string>{"Alice","Bob"}, "Saved identities must count once, case-insensitive ranks");
        check(directory.at("futurerank").members == std::vector<std::string>{"Offline"}, "Unknown/future/offline ranks must not disappear");
        check(directory.at("owner").members.empty(), "Empty configured ranks must remain visible");
        check(utils::markedNickname("Name") == "~Name" && utils::markedNickname("~~Name") == "~Name", "Nickname marker must be visible and idempotent");
        check(utils::markedNickname("§bName") == "~§bName", "Colored nickname marker");
        check(utils::staffChatMessage("~Nick","{hello}") == "§8[§cStaff§8] §e~Nick§7: §f{hello}", "Staff colors, nicknames and literal braces");
        std::cout << "Rank delegation, membership, scoped mute and display regression tests passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
