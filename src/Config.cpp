#include "ContainerPreview/Config.h"
#include <json.hpp>
#include <cctype>
#include <string>
namespace preview {
namespace {
std::optional<unsigned> Key(std::string key) {
    for(auto& ch:key) ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if(key=="LALT" || key=="LEFT_ALT")return 0xA4;
    if(key=="RALT" || key=="RIGHT_ALT")return 0xA5;
    if(key=="LCTRL")return 0xA2;
    if(key=="RCTRL")return 0xA3;
    if(key=="LSHIFT")return 0xA0;
    if(key=="RSHIFT")return 0xA1;
    if(key=="SPACE")return 0x20;
    if(key.size()==1 && ((key[0]>='A' && key[0]<='Z') || (key[0]>='0' && key[0]<='9')))return static_cast<unsigned>(key[0]);
    if(key.size()>=2 && key.size()<=3 && key[0]=='F') {
        unsigned number{};
        for(std::size_t i=1;i<key.size();++i) {
            if(key[i]<'0' || key[i]>'9')return {};
            number=number*10+static_cast<unsigned>(key[i]-'0');
        }
        if(number>=1 && number<=24)return 0x6F+number;
    }
    return {};
}
}
std::optional<Config> ParseConfig(std::string_view text) {
    if(text.size()>4096)return {};
    auto json=nlohmann::json::parse(text,nullptr,false);
    if(!json.is_object())return {};
    Config config;
    if(json.contains("enabled")) {
        if(!json["enabled"].is_boolean())return {};
        config.enabled=json["enabled"].get<bool>();
    }
    if(json.contains("holdKey")) {
        if(!json["holdKey"].is_string())return {};
        auto key=Key(json["holdKey"].get<std::string>());
        if(!key)return {};
        config.holdKey=*key;
    }
    return config;
}
bool ReloadConfig(Config& current, std::string_view text) {
    auto next=ParseConfig(text);
    if(!next)return false;
    current=*next;
    return true;
}
}
