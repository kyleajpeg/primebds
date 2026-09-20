#include "primebds/utils/hierarchy_policy.h"
#include "primebds/utils/spy_policy.h"
#include <cstdlib>
#include <iostream>

using namespace primebds::hierarchy;
static void check(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}
int main() {
    const Rank owner{"Owner",1000}, op{"Operator",100}, co{"Co-Owner",50}, mod{"Moderator",25}, member{"Default",0};
    const std::vector<Rank> ranks{owner,op,co,mod,member};
    for (std::size_t i=0;i<ranks.size();++i) for (std::size_t j=0;j<ranks.size();++j) {
        check(canTarget(ranks[i],ranks[j],false,true)==(i<2 || i<j),"target rank order/peer protection");
        check(canTarget(ranks[i],ranks[i],true,true),"self utility use");
        check(canTarget(ranks[i],ranks[i],true,false)==(i<2),"self moderation/assignment protection");
        for (std::size_t k=0;k<ranks.size();++k)
            check(canObserve(ranks[i],{ranks[j],ranks[k]})==(i<2 || (i<j && i<k)),"both spy participants must be lower");
    }
    check(!canObserve(mod,{owner,member}),"Owner -> Default privacy");
    check(!canObserve(mod,{member,owner}),"Default -> Owner privacy");
    check(!canObserve(mod,{mod,member}),"peer moderation privacy");
    check(canObserve(owner,{owner,member}),"Owner observer bypass");
    check(canObserve(mod,{member,member}),"lower-only conversation visible");
    check(!canObserve(owner,{}),"unattributed event must not leak");
    check(canTarget(owner,{"Unknown",std::nullopt},false,true),"trusted recovery can target unknown weight");
    check(!canTarget({"Broken",std::nullopt},member,false,true),"missing actor weight");
    check(!outranks({"Fake",2000},owner),"misconfigured higher weight cannot target Owner");
    check(!outranks({"Fake",2000},op),"misconfigured higher weight cannot target Operator");
    check(!outranks({"Moderator",25},{"DifferentPeer",25}),"different ranks at equal weight");
    check(canAssign(co,member,mod,false,false),"delegated lower promotion");
    check(!canAssign(co,owner,member,false,false),"cannot demote higher target");
    check(!canAssign(co,member,co,false,false),"cannot assign equal rank");
    check(!canAssign(co,member,op,false,true),"cannot assign protected rank");
    check(!canAssign(co,member,mod,false,true),"lower weight op rank escape");
    check(!canAssign(co,member,mod,true,false),"cannot change own rank");
    check(canAssign(owner,member,op,false,true),"Owner can appoint lower Operator");
    check(canAssign(owner,member,owner,false,true),"Owner can appoint Owner");
    check(canAssign(op,owner,op,true,true),"Operator can change own/protected ranks");
    check(canAssign(owner,owner,member,true,false),"Owner can demote self");
    check(!privileged({"Admin",1000}) && !privileged({"Owner-lookalike",1000}),"only exact privileged roles bypass");
    check(!canTarget(mod,{"Unknown",std::nullopt},false,true),"ordinary missing target fails closed");
    check(canAccessWarnings(false,true,false,true,false,false),"member reads own warnings");
    check(!canAccessWarnings(false,true,true,true,false,false),"member cannot erase warnings");
    check(!canAccessWarnings(false,false,false,true,false,true),"self node cannot read others");
    check(canAccessWarnings(false,false,true,false,true,true),"moderator edits lower warnings");
    check(!canAccessWarnings(false,false,true,false,true,false),"moderator cannot edit protected warnings");
    check(canAccessWarnings(true,true,true,false,false,false),"admin warning bypass");
    check(spyTargets("tell",{"Owner","private message"},"")->at(0)=="Owner","private recipient included");
    check(spyTargets("reply",{"private message"},"Owner")->at(0)=="Owner","reply recipient included");
    check(!spyTargets("reply",{"message"},""),"unresolved reply is private");
    check(!spyTargets("execute",{"as","Owner"},""),"indirect commands withheld from non-admin spies");
    check(!spyTargets("staffchat",{"staff message"},""),"staff channel withheld from non-admin command spies");
    check(spyTargets("speed",{"2","Owner"},"")->at(0)=="Owner","speed target included for privacy");
    auto t=tokenize("/minecraft:gamemode creative \"Player With Spaces\"");
    check(t && t->size()==3 && (*t)[2]=="Player With Spaces","quoted native target");
    check(!tokenize("rank set \"unfinished"),"malformed quotes fail closed");
    check(canonicalName("/MINECRAFT:Op")=="op","namespaced alias canonicalization");
    check(canonicalName("other:op")=="other:op","unrecognized namespace stays unrecognized");
    check(commandPolicy("future_unreviewed_command")==CommandPolicy::Deny,"new commands fail closed");
    check(commandPolicy("ipban")==CommandPolicy::Console,"IP collateral cannot bypass hierarchy");
    check(teleportTargets({"10","64","-10"})->empty(),"self coordinate teleport for homes/warps");
    check(teleportTargets({"~","~1","~-1","90","0","true"})->empty(),"relative coordinates and rotation");
    check(teleportTargets({"Member","1","2","3"})->at(0)=="Member","coordinate victim extracted");
    check(teleportTargets({"Owner"})->at(0)=="Owner","single destination must be checked");
    check(teleportTargets({"Member","Owner"})->size()==2,"both teleport identities checked");
    check(!teleportTargets({"@a","1","2","3"}),"native selector teleport denied");
    check(!teleportTargets({"Member","~","~","~","facing","Owner"}),"unreviewed facing forms denied");
    check(!teleportTargets({"1","2","Owner"}),"coordinate/name ambiguity denied");
    check(!coordinate("~-") && !coordinate("nan") && !coordinate("1;op"),"malformed coordinates denied");
    std::cout << "Hierarchy target, assignment, privacy and parser policy tests passed.\n";
}
