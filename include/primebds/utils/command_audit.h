#pragma once

#include <string>
#include <string_view>

namespace primebds::utils {

// Preserve submitted text while preventing forged lines, terminal escapes or
// Minecraft formatting in the audit record. Never modify the actual command.
inline std::string quoteAuditText(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '\\' || c == '"') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 0x20 || c == 0x7f) {
            result += "\\x";
            result += hex[c >> 4];
            result += hex[c & 0xf];
        } else if (c == 0xc2 && i + 1 < text.size() &&
                   (static_cast<unsigned char>(text[i + 1]) == 0xa7 ||
                    (static_cast<unsigned char>(text[i + 1]) >= 0x80 &&
                     static_cast<unsigned char>(text[i + 1]) <= 0x9f))) {
            const auto next = static_cast<unsigned char>(text[++i]);
            result += "\\u00";
            result += hex[next >> 4];
            result += hex[next & 0xf];
        } else if (c == 0xe2 && i + 2 < text.size() &&
                   static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(text[i + 2]) == 0xa8 ||
                    static_cast<unsigned char>(text[i + 2]) == 0xa9)) {
            result += static_cast<unsigned char>(text[i + 2]) == 0xa8 ? "\\u2028" : "\\u2029";
            i += 2;
        } else {
            result += static_cast<char>(c);
        }
    }
    result += '"';
    return result;
}

inline std::string formatCommandAttempt(std::string_view player, std::string_view command) {
    return "[CommandAudit] player=" + quoteAuditText(player) +
           " attempted=" + quoteAuditText(command);
}

} // namespace primebds::utils
