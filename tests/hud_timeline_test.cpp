#include "primebds/utils/hud_timeline.h"

#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace primebds::utils;

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

static std::string bytes(std::initializer_list<unsigned> values) {
    std::string result;
    for (const auto value : values) result += static_cast<char>(value);
    return result;
}

static void checkDecoding() {
    const std::vector<std::string> names{"Unknown", "Start", "End"};
    for (unsigned type = 0; type < names.size(); ++type) {
        const auto without_id = bytes({type * 2, 0});
        const auto result = decodeHudLoadingPacket(without_id);
        check(result.decoded && result.type == static_cast<std::int32_t>(type) &&
                  result.typeName() == names[type] && !result.screen_id && result.error.empty(),
              "Known signed-varint types decode without an optional ID");
        const auto with_id = bytes({type * 2, 1, 0x78, 0x56, 0x34, 0x12});
        const auto screen = decodeHudLoadingPacket(with_id);
        check(screen.decoded && screen.type == static_cast<std::int32_t>(type) &&
                  screen.typeName() == names[type] && screen.screen_id == 0x12345678u && screen.error.empty(),
              "Each known type accepts a complete little-endian optional screen ID");
        check(without_id == bytes({type * 2, 0}) && with_id == bytes({type * 2, 1, 0x78, 0x56, 0x34, 0x12}),
              "Decoding leaves the supplied payload unchanged");
    }
    check(decodeHudLoadingPacket(bytes({4, 1, 0, 0, 0, 0})).screen_id == 0u,
          "Present zero ID is distinct from absent ID");
    check(decodeHudLoadingPacket(bytes({4, 1, 255, 255, 255, 255})).screen_id ==
              std::numeric_limits<std::uint32_t>::max(), "Screen IDs retain their full unsigned width");
    check(decodeHudLoadingPacket(bytes({0x84, 0x80, 0x80, 0x80, 0, 0})).decoded,
          "A bounded five-byte varint representation can be read without overflow");

    const auto complete = bytes({4, 1, 0x78, 0x56, 0x34, 0x12});
    for (std::size_t length = 0; length < complete.size(); ++length) {
        const auto result = decodeHudLoadingPacket(std::string_view(complete).substr(0, length));
        check(!result.decoded && !result.screen_id && !result.error.empty() &&
                  std::string(result.typeName()) == "Undecoded", "Every incomplete prefix remains undecoded");
    }
    struct InvalidCase { std::string payload; const char *error; };
    const std::vector<InvalidCase> invalid{
        {bytes({0x80}), "truncated-type"},
        {bytes({0x80, 0x80, 0x80, 0x80}), "truncated-type"},
        {bytes({0x80, 0x80, 0x80, 0x80, 0x10, 0}), "overflow-type"},
        {bytes({0x80, 0x80, 0x80, 0x80, 0x80, 0, 0}), "overflow-type"},
        {bytes({0xff, 0xff, 0xff, 0xff, 0xff, 0}), "overflow-type"},
        {bytes({6, 0}), "unknown-type"},
        {bytes({1, 0}), "unknown-type"},
        {bytes({0xfe, 0xff, 0xff, 0xff, 0x0f, 0}), "unknown-type"},
        {bytes({0xff, 0xff, 0xff, 0xff, 0x0f, 0}), "unknown-type"},
        {bytes({4}), "missing-screen-id-presence"},
        {bytes({4, 2}), "invalid-screen-id-presence"},
        {bytes({4, 255}), "invalid-screen-id-presence"},
        {bytes({4, 1, 0, 0, 0}), "truncated-screen-id"},
        {bytes({4, 0, 0}), "trailing-data"},
        {bytes({4, 1, 0, 0, 0, 0, 0}), "trailing-data"}
    };
    for (const auto &item : invalid) {
        const auto original = item.payload;
        const auto result = decodeHudLoadingPacket(item.payload);
        check(!result.decoded && result.error == item.error && !result.screen_id &&
                  std::string(result.typeName()) == "Undecoded", "Malformed or unfamiliar formats stay explicitly undecoded");
        check(item.payload == original, "Rejected payloads also remain unchanged");
    }
    check(decodeHudLoadingPacket(bytes({0xff, 0xff, 0xff, 0xff, 0x0f, 0})).type ==
              std::numeric_limits<std::int32_t>::min(), "Zigzag decoding handles signed minimum without overflow");
}

static void checkHex() {
    check(hudPayloadHex("").empty(), "Empty payload has empty hexadecimal output");
    check(hudPayloadHex(bytes({0, 0x0f, 0x10, 0x7f, 0x80, 0xff})) == "000f107f80ff",
          "Hex output is lowercase and treats binary bytes as unsigned");
    std::string payload, expected;
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned byte = 0; byte < 256; ++byte) {
        payload += static_cast<char>(byte);
        if (byte < 32) {
            expected += digits[byte >> 4];
            expected += digits[byte & 15];
        }
    }
    const auto original = payload;
    check(hudPayloadHex(payload) == expected && expected.size() == 64,
          "Payload logging emits exactly the first 32 bytes even for large input");
    check(hudPayloadHex(std::string_view(payload).substr(0, 31)).size() == 62 &&
              hudPayloadHex(std::string_view(payload).substr(0, 32)) == expected &&
              hudPayloadHex(std::string_view(payload).substr(0, 33)) == expected,
          "Hex boundary at 32 bytes is exact");
    check(payload == original, "Hex formatting does not mutate packet contents");
}

static void checkSessions() {
    HudTimeline timeline;
    check(timeline.generation() == 1 && timeline.session("a") == 0 && !timeline.active("a", 0),
          "No session is represented by zero; missing sessions never count as active");
    const auto first = timeline.start("a");
    const auto other = timeline.start("b");
    check(first != 0 && other > first && timeline.active("a", first) && timeline.active("b", other) &&
              !timeline.active("a", other), "Login IDs increase globally and remain bound to the correct player");
    const auto reconnect = timeline.start("a");
    check(reconnect > other && !timeline.active("a", first) && timeline.active("a", reconnect),
          "A reconnect invalidates stale callbacks even when UUID is unchanged");
    timeline.end("a");
    check(timeline.session("a") == 0 && !timeline.active("a", reconnect) && timeline.active("b", other),
          "Disconnect expires only the departing session");
    timeline.end("absent");
    check(timeline.active("b", other), "Unassociated disconnect does not expire another player");
    const auto later = timeline.start("a");
    const auto generation = timeline.generation();
    const auto elapsed = timeline.elapsedMs();
    timeline.clear();
    check(timeline.generation() == generation + 1 && !timeline.active("a", later) &&
              !timeline.active("b", other) && timeline.elapsedMs() >= elapsed,
          "Plugin shutdown clears sessions and expires unassociated callbacks without resetting elapsed time");
    const auto after_clear = timeline.start("a");
    check(after_clear > later && !timeline.active("a", later) && timeline.active("a", after_clear),
          "Re-enabling cannot reuse an earlier login ID or revive a stale marker");
    timeline.clear();
    check(timeline.generation() == generation + 2, "Each bookkeeping reset expires the prior callback generation");
    const auto start = timeline.elapsedMs();
    check(timeline.elapsedMs() >= start, "Diagnostic elapsed time is monotonic");
}

int main() {
    try {
        checkDecoding();
        checkHex();
        checkSessions();
        std::cout << "HUD loading decoder, bounded logging and login-session lifetime tests passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
