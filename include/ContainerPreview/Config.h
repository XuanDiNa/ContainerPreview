#pragma once
#include <optional>
#include <string_view>
namespace preview {
struct Config {
    bool enabled = true;
    unsigned holdKey = 0xA4; // VK_LMENU
    unsigned holdModifiers{};
    bool operator==(Config const&) const = default;
};
std::optional<Config> ParseConfig(std::string_view text);
bool ReloadConfig(Config& current, std::string_view text);
}
