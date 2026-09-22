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
        check(utils::mayUseRankAction(false,"list",has), "List-only delegation must work");
        for (auto action : {"set","info","create","delete","perm","inherit","weight","prefix","suffix",""})
            check(!utils::mayUseRankAction(false,action,has), "Read delegation leaked another subcommand");
        grants.insert("primebds.command.rank.info");
        grants.insert("primebds.command.rank.set");
        check(utils::mayUseRankAction(false,"info",has) && utils::mayUseRankAction(false,"set",has), "Independent delegated nodes");
        check(!utils::mayUseRankAction(false,"delete",has), "Delegated rank editing must be denied");
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
        check(utils::mayLiftGlobalMute(false,true,false,admin,admin), "Issuer can undo own mute");
        check(!utils::mayLiftGlobalMute(false,false,false,admin,admin), "Peer can lift another peer's mute");
        check(!utils::mayLiftGlobalMute(false,false,false,admin,co), "Lower rank can lift higher mute");
        check(utils::mayLiftGlobalMute(false,false,false,co,admin), "Higher staff can lift lower mute");
        check(!utils::mayLiftGlobalMute(false,false,true,co,admin), "Console mute bypassed");
        check(utils::mayLiftGlobalMute(true,false,true,owner,owner), "Owner recovery bypass retained");
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
