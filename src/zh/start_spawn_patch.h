#pragma once
#include "sco/scan.h"
#include <array>
#include <cstring>
#include <limits>

namespace sco_start {
struct SpawnDefault {
    uint8_t* instruction=nullptr;
    const uint8_t* name=nullptr;
    std::array<uint8_t,7> replacement{};
    int matches=0;
    const char* reason="registration not found";
};
inline bool Contains(const sco::Section& s,const uint8_t* p,size_t n=1){
    const auto base=reinterpret_cast<uintptr_t>(s.base),at=reinterpret_cast<uintptr_t>(p);
    return s.base && at>=base && at-base<=s.size && n<=s.size-(at-base);
}
// Resolve against original executable bytes. No fixed addresses, writes or engine calls.
// All three literals, the string cvar overload, flags, and the empty default must agree.
inline SpawnDefault LocateSpawnDefault(const sco::Section& text,const sco::Section& rdata,const char* cityName="Stanton4_NewBabbage",const uint8_t* nearName=nullptr){
    SpawnDefault result;
    const auto* name=sco::FindCString(rdata,"sv_OverridePlayerPersistSpawnLocation");
    const auto* help=sco::FindCString(rdata,"Server only: Override the spawn location for all players");
    const auto* city=nearName?nearName:sco::FindCString(rdata,cityName);
    if(!name||!help){result.reason="required spawn literals missing";return result;}
    uint8_t* candidates[512]={};
    const int count=sco::FindPattern(text,
        "4C 8D 0D ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 89 54 24 28 "
        "4C 8D 05 ?? ?? ?? ?? 48 8B 01 48 8D 15 ?? ?? ?? ?? "
        "C7 44 24 20 08 00 00 00 FF 50 38",candidates,512);
    if(count>512){result.reason="too many registration candidates";return result;}
    uint8_t* selected=nullptr;
    for(int i=0;i<count;++i){
        auto*p=candidates[i];
        if(sco::RipTarget(p+7,3,7)!=help || sco::RipTarget(p+29,3,7)!=name)continue;
        const auto* initial=sco::RipTarget(p,3,7);
        if(!Contains(rdata,initial)||*initial)continue;
        ++result.matches;selected=p;
    }
    if(result.matches!=1){result.reason="spawn registration is not unique or its default changed";return result;}
    result.instruction=selected;
    if(!city){result.reason="destination needs a near-memory string";return result;}
    const intptr_t delta=reinterpret_cast<intptr_t>(city)-reinterpret_cast<intptr_t>(selected+7);
    if(delta<(std::numeric_limits<int32_t>::min)()||delta>(std::numeric_limits<int32_t>::max)()){
        result.reason="spawn literal is outside relative instruction range";return result;
    }
    std::memcpy(result.replacement.data(),selected,7);const auto disp=static_cast<int32_t>(delta);
    std::memcpy(result.replacement.data()+3,&disp,4);
    result.instruction=selected;result.name=city;result.reason=nullptr;return result;
}
}
