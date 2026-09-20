#include "primebds/utils/hierarchy_policy.h"
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
        check(canTarget(ranks[i],ranks[j],false,true)==(i<j),"target rank order/peer protection");
        check(canTarget(ranks[i],ranks[i],true,true),"self utility use");
        check(!canTarget(ranks[i],ranks[i],true,false),"self moderation/assignment protection");
        for (std::size_t k=0;k<ranks.size();++k)
            check(canObserve(ranks[i],{ranks[j],ranks[k]})==(i<j && i<k),"both spy participants must be lower");
    }
    check(!canObserve(mod,{owner,member}),"Owner -> Default privacy");
    check(!canObserve(mod,{member,owner}),"Default -> Owner privacy");
    check(!canObserve(mod,{mod,member}),"peer moderation privacy");
    check(!canObserve(owner,{owner,member}),"Owner peer privacy");
    check(canObserve(mod,{member,member}),"lower-only conversation visible");
    check(!canObserve(owner,{}),"unattributed event must not leak");
    check(!canTarget(owner,{"Unknown",std::nullopt},false,true),"missing target weight");
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
    check(!canAssign(owner,member,owner,false,true),"Owner appointment requires panel");
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
