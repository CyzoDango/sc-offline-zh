#pragma once
#include "fleet_metadata.generated.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
namespace sco_zh {
inline std::string FleetLower(std::string s){for(char&c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline const HullMeta* Hull(const char* cls){
    static const auto index=[] {std::unordered_map<std::string,const HullMeta*> out;for(const auto&h:kHullMetadata)out.emplace(FleetLower(h.cls),&h);return out;}();
    const auto it=index.find(FleetLower(cls?cls:""));return it==index.end()?nullptr:it->second;
}
inline std::string ShipBase(std::string cls){
    const std::string lower=FleetLower(cls);
    size_t at=0;
    while((at=lower.find('_',at))!=std::string::npos){
        const size_t end=lower.find('_',at+1);
        const std::string token=lower.substr(at+1,end-at-1);
        for(const char* word:{"pu","ea","ai","ac","unmanned","derelict","wreck","template","mission","display","bmbrck","temp"})
            if(token==word)return cls.substr(0,at);
        ++at;
    }
    return cls;
}
// All / original / PU / EA / AI / unmanned-wreck-test; filters may overlap.
inline bool VersionMatches(const char* cls,int filter){
    if(!filter)return true;
    const std::string n=FleetLower(cls),base=FleetLower(ShipBase(cls));
    const std::string tail=n.substr(base.size());
    auto token=[&](const char* t){return (tail+"_").find(std::string("_")+t+"_")!=std::string::npos;};
    if(filter==1)return tail.empty();
    if(filter==2)return token("pu");
    if(filter==3)return token("ea")||token("ac");
    if(filter==4)return token("ai");
    return token("unmanned")||token("derelict")||token("wreck")||token("template")||token("display")||token("bmbrck")||token("temp");
}
inline std::string VariantName(const char* cls){
    const std::string n=cls?cls:"",base=ShipBase(n);
    if(n==base)return "原版";
    static const std::unordered_map<std::string,const char*> words={
        {"zh","本地改装"},{"ballistic","实弹"},{"pu","PU"},{"ea","EA"},{"ac","AC"},{"ai","AI"},{"civ","平民"},{"crim","罪犯"},{"pir","海盗"},
        {"uee","帝国海军"},{"van","剜度"},{"nt","九尾"},{"ninetails","九尾"},{"xenothreat","异种威胁"},
        {"ff","边境战士"},{"sec","安保"},{"super","高级"},{"def","防御"},{"alpha","阿尔法"},{"hunter","猎人"},
        {"unmanned","无人"},{"salvage","打捞"},{"template","模板"},{"derelict","废弃"},{"wreck","残骸"},
        {"display","展示"},{"mission","任务"},{"boarded","登船"},{"shipboarded","登船"},{"bh","赏金猎人"},
        {"advocacy","倡导局"},{"fleetweek","舰队周"},{"temp","测试"},{"nointerior","无内饰"}};
    std::string result;size_t at=base.size()+1;
    while(at<n.size()){
        size_t end=n.find('_',at);const std::string t=n.substr(at,end-at);
        const auto it=words.find(FleetLower(t));if(!result.empty())result+=' ';
        result+=it==words.end()?t:it->second;if(end==std::string::npos)break;at=end+1;
    }
    return result;
}
inline std::string FleetRecipe(const char* cls,int relation,int crew){
    if(relation==0&&crew<0)return cls?cls:""; // legacy callers keep their original behaviour
    const auto* h=Hull(cls);
    if(!h||!h->ready||relation<0||relation>3||crew<0||crew>1)return {};
    char out[64];snprintf(out,sizeof(out),"SCOZH_S%04d_%c%d",h->recipe,"OENF"[relation],crew);return out;
}
inline double ShipSpacing(double width,double length,double height,double extra){
    const double diameter=std::sqrt(width*width+length*length+height*height);
    return (std::max)(10.0,std::isfinite(diameter)?diameter+10.0:1500.0)+(std::max)(0.0,std::isfinite(extra)?extra:0.0);
}
using FleetVec=std::array<double,3>;
inline void ShipAxes(const double q[4],FleetVec& right,FleetVec& forward,FleetVec& up){
    right={1-2*(q[1]*q[1]+q[2]*q[2]),2*(q[0]*q[1]+q[3]*q[2]),2*(q[0]*q[2]-q[3]*q[1])};
    forward={2*(q[0]*q[1]-q[3]*q[2]),1-2*(q[0]*q[0]+q[2]*q[2]),2*(q[1]*q[2]+q[3]*q[0])};
    up={2*(q[0]*q[2]+q[3]*q[1]),2*(q[1]*q[2]-q[3]*q[0]),1-2*(q[0]*q[0]+q[1]*q[1])};
}
// Direction: up, down, right, left, front, back. First hull is at the chosen distance;
// later hulls extend sideways in an integer grid. Basis and origin are captured once.
inline FleetVec ShipOffset(int direction,double distance,int ordinal,int count,double spacing,const double q[4]){
    FleetVec right,forward,up;ShipAxes(q,right,forward,up);
    const FleetVec axis=direction<2?up:direction<4?right:forward;
    const FleetVec across=direction<2?right:direction<4?forward:right;
    const FleetVec rows=direction<2?forward:up;
    const double sign=(direction%2)?-1:1;
    const int cols=(std::max)(1,static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    FleetVec out{};
    for(int k=0;k<3;++k)out[k]=sign*distance*axis[k]+spacing*((ordinal%cols)*across[k]+(ordinal/cols)*rows[k]);
    return out;
}
} // namespace sco_zh
