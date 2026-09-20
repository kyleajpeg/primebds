#include "primebds/utils/warning_command.h"
#include <iostream>
#include <stdexcept>
using namespace primebds::utils;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
int main() {
    try {
        const auto timed = parseWarning({"Player Name","2","minutes","Repeated griefing"}, 1000);
        check(timed && timed->target == "Player Name" && timed->expires_at == 1120 && timed->reason == "Repeated griefing", "Plural unit and message argument");
        const auto split = parseWarning({"Member","1","day","Repeated","griefing"}, 1000);
        check(split && split->expires_at == 87400 && split->reason == "Repeated griefing", "Tokenized console reason");
        const auto permanent = parseWarning({"Member","permanent","Griefing for 2 minutes"}, 1000);
        check(permanent && permanent->expires_at == 0 && permanent->reason == "Griefing for 2 minutes", "Explicit permanence; reason suffix is untouched");
        for (const auto &args : std::vector<std::vector<std::string>>{
            {}, {"Member"}, {"Member","reason"}, {"Member","reason","2","minutes"},
            {"Member","2","minuts","reason"}, {"Member","2","minutess","reason"},
            {"Member","two","minutes","reason"}, {"Member","0","minutes","reason"},
            {"Member","-2","minutes","reason"}, {"Member","1.5","minutes","reason"},
            {"Member","2x","minutes","reason"}, {"Member","99999999999999999999999","years","reason"},
            {"Member","9223372036854775807","years","reason"}, {"Member","permanant","reason"},
            {"Member","permanent"}, {"Member","permanent","   "}, {"Member","2","minutes"}})
            check(!parseWarning(args,1000), "Malformed input must not create a warning");
        for (const auto &unit : {"second","minute","hour","day","week","month","year"}) {
            const auto single = parseWarning({"Member","1",unit,"reason"},1000);
            const auto plural = parseWarning({"Member","1",std::string(unit)+"s","reason"},1000);
            check(single && plural && single->expires_at == plural->expires_at, "All singular/plural durations agree");
        }
        std::cout << "Strict warning duration and reason parsing passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
