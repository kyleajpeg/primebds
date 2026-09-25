#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace primebds::utils {

struct HudLoadingPacket {
    bool decoded = false;
    std::int32_t type = 0;
    std::optional<std::uint32_t> screen_id;
    std::string error;

    const char *typeName() const {
        if (!decoded) return "Undecoded";
        switch (type) {
            case 0: return "Unknown";
            case 1: return "Start";
            case 2: return "End";
            default: return "Undecoded";
        }
    }
};

// Expected ServerboundLoadingScreenPacket payload: signed-varint32 type,
// presence bool, and (when present) a little-endian uint32 screen ID. Decode
// only that complete layout; this observer must never reinterpret partial or
// future packet formats as an End signal.
inline HudLoadingPacket decodeHudLoadingPacket(std::string_view payload) {
    HudLoadingPacket result;
    std::size_t offset = 0;
    std::uint32_t encoded = 0;
    bool terminated = false;
    for (unsigned index = 0; index < 5; ++index) {
        if (offset == payload.size()) {
            result.error = "truncated-type";
            return result;
        }
        const auto byte = static_cast<unsigned char>(payload[offset++]);
        if (index == 4 && (byte & 0xf0u) != 0) {
            result.error = "overflow-type";
            return result;
        }
        encoded |= static_cast<std::uint32_t>(byte & 0x7fu) << (index * 7);
        if ((byte & 0x80u) == 0) {
            terminated = true;
            break;
        }
    }
    if (!terminated) {
        result.error = "overflow-type";
        return result;
    }
    // Avoid unsigned-to-signed overflow when decoding negative values.
    result.type = (encoded & 1u) ? -static_cast<std::int32_t>(encoded >> 1) - 1
                                : static_cast<std::int32_t>(encoded >> 1);
    if (result.type < 0 || result.type > 2) {
        result.error = "unknown-type";
        return result;
    }
    if (offset == payload.size()) {
        result.error = "missing-screen-id-presence";
        return result;
    }
    const auto present = static_cast<unsigned char>(payload[offset++]);
    if (present > 1) {
        result.error = "invalid-screen-id-presence";
        return result;
    }
    std::optional<std::uint32_t> screen_id;
    if (present) {
        if (payload.size() - offset < 4) {
            result.error = "truncated-screen-id";
            return result;
        }
        std::uint32_t value = 0;
        for (unsigned index = 0; index < 4; ++index)
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(payload[offset++])) << (index * 8);
        screen_id = value;
    }
    if (offset != payload.size()) {
        result.error = "trailing-data";
        return result;
    }
    result.screen_id = screen_id;
    result.decoded = true;
    return result;
}

inline std::string hudPayloadHex(std::string_view payload) {
    constexpr char digits[] = "0123456789abcdef";
    const auto length = payload.size() < 32 ? payload.size() : 32;
    std::string result;
    result.reserve(length * 2);
    for (std::size_t index = 0; index < length; ++index) {
        const auto byte = static_cast<unsigned char>(payload[index]);
        result += digits[byte >> 4];
        result += digits[byte & 0x0f];
    }
    return result;
}

// Accessed on the server thread. IDs correlate records and guard scheduled
// work against stale login sessions; packet observations never gate gameplay.
class HudTimeline {
public:
    std::uint64_t elapsedMs() const {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - origin_).count());
    }

    std::uint64_t start(const std::string &uuid) {
        const auto id = ++sequence_;
        sessions_[uuid] = id;
        return id;
    }

    std::uint64_t session(const std::string &uuid) const {
        const auto found = sessions_.find(uuid);
        return found == sessions_.end() ? 0 : found->second;
    }

    bool active(const std::string &uuid, std::uint64_t id) const {
        return id != 0 && session(uuid) == id;
    }

    void end(const std::string &uuid) { sessions_.erase(uuid); }

    void clear() {
        sessions_.clear();
        ++generation_;
    }

    std::uint64_t generation() const { return generation_; }

private:
    const std::chrono::steady_clock::time_point origin_ = std::chrono::steady_clock::now();
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 1;
    std::unordered_map<std::string, std::uint64_t> sessions_;
};

} // namespace primebds::utils
