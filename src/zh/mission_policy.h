#pragma once
#include <string>
#include <string_view>
#include <cctype>
#include <initializer_list>
namespace sco_zh {
inline std::string MissionLower(std::string_view value){std::string out(value);for(char&c:out)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return out;}
inline int MissionCategory(std::string_view value){
    const auto s=MissionLower(value);auto any=[&](std::initializer_list<const char*> keys){for(auto k:keys)if(s.find(k)!=std::string::npos)return true;return false;};
    if(any({"haul","cargo","delivery","courier","supply","ato b","atob","firesale"}))return 1;
    if(any({"shipambush","shipwave","supportattacked","killship","bounty","shipbattle","spacecombat","patrol","escortship"}))return 2;
    if(any({"eliminate","assassin","facility","bunker","fps","killanimal","killkopion","killwm","outposts","mercenary","sweep"}))return 3;
    return 4;
}
inline bool MissionSystemMatches(std::string_view name,std::string_view system){
    const auto n=MissionLower(name),s=MissionLower(system);
    for(auto key:{"stanton","pyro","nyx"})if(n.find(key)!=std::string::npos)return s==key;
    return true;
}
}
