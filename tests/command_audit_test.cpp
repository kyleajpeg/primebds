#include "primebds/utils/command_audit.h"

#include <cstdlib>
#include <iostream>
#include <string>

static void check(bool condition, const char *message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    using primebds::utils::formatCommandAttempt;
    using primebds::utils::quoteAuditText;
    check(formatCommandAttempt("Kyle", "/op Kyle") ==
          "[CommandAudit] player=\"Kyle\" attempted=\"/op Kyle\"", "attempt record");
    check(quoteAuditText("/unknown  {arg}  ") == "\"/unknown  {arg}  \"", "preserve input");
    check(quoteAuditText("a\r\nb\t\x1b[31m") == "\"a\\x0d\\x0ab\\x09\\x1b[31m\"", "escape controls");
    check(quoteAuditText(std::string("a\0b", 3)) == "\"a\\x00b\"", "embedded NUL");
    check(quoteAuditText("\"\\") == "\"\\\"\\\\\"", "quotes and backslashes");
    check(quoteAuditText("\xc2\xa7" "cRed") == "\"\\u00a7cRed\"", "escape Minecraft colors");
    check(quoteAuditText("\xc2\x85\xe2\x80\xa8\xe2\x80\xa9") ==
          "\"\\u0085\\u2028\\u2029\"", "escape Unicode line breaks");
    check(quoteAuditText("caf\xc3\xa9") == "\"caf\xc3\xa9\"", "preserve Unicode");
    const std::string long_command = "/tell Kyle " + std::string(10000, 'a');
    check(quoteAuditText(long_command) == "\"" + long_command + "\"", "no silent truncation");
    std::cout << "Command audit tests passed\n";
}
