#pragma once
#include <string_view>
#include "star_selection.h"

namespace sco_start {
enum class Location { Default, Daymar, NewBabbage, StarMap };
template<class C> inline bool Equal(std::basic_string_view<C> text, const char* expected) {
    size_t i=0;
    for(;i<text.size() && expected[i];++i){
        C c=text[i];if(c>=C('A')&&c<=C('Z'))c+=C('a'-'A');
        char e=expected[i];if(e>='A'&&e<='Z')e+='a'-'A';
        if(c!=C(e))return false;
    }
    return i==text.size() && !expected[i];
}
template<class C> inline Location Parse(std::basic_string_view<C> value) {
    if(Equal(value,"NewBabbage"))return Location::NewBabbage;
    if(Equal(value,"Daymar"))return Location::Daymar;
    if(Equal(value,"StarMap"))return Location::StarMap;
    return Location::Default;
}
inline const char* NativeName(std::string_view value,std::string_view target={}) {
    if(Parse(value)==Location::NewBabbage)return "Stanton4_NewBabbage";
    if(Parse(value)==Location::StarMap){
        if(int p=sco_map::FindPlace(target);p>=0)return sco_map::kPlaces[p].key;
        if(int b=sco_map::FindBody(target);b>=0 && sco_map::kBodies[b].surface)return sco_map::kSystems[sco_map::kBodies[b].system].native;
    }
    return nullptr;
}
inline std::wstring NativeArguments(std::wstring_view value,std::wstring_view target={}) {
    const char* native=NativeName(sco_map::Narrow(value),sco_map::Narrow(target));
    return native?L" +sv_OverridePlayerPersistSpawnLocation "+sco_map::Wide(native):L"";
}
inline bool OverridesSavedSpot(std::string_view value) { return Parse(value)!=Location::Default; }
}
