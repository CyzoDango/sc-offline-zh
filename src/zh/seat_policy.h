#pragma once
#include <string>
#include <cctype>
namespace sco_zh {
inline bool NamedPilot(const char* name){
    std::string s=name?name:"";for(char&c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string joined;for(unsigned char c:s)if(std::isalnum(c))joined+=static_cast<char>(c);
    if(joined.find("copilot")!=std::string::npos || joined.find("secondarypilot")!=std::string::npos ||
       joined.find("auxiliarypilot")!=std::string::npos)return false;
    for(size_t at=0;(at=s.find("pilot",at))!=std::string::npos;++at){
        const bool left=at==0||!std::isalnum(static_cast<unsigned char>(s[at-1]));
        const bool right=at+5==s.size()||!std::isalnum(static_cast<unsigned char>(s[at+5]));
        if(left&&right)return true;
    }
    return false;
}
inline unsigned SeatRank(const char* name,unsigned priority){return (NamedPilot(name)?0x80000000u:0u)|(priority&0x7fffffffu);}
template<class Seat> int PrimaryPilotIndex(const Seat* seats,int count){
    int best=-1;bool ambiguous=false;
    for(int i=0;i<count;++i){
        if(seats[i].usable==0 || !NamedPilot(seats[i].name))continue;
        if(best<0 || seats[i].priority>seats[best].priority){best=i;ambiguous=false;}
        else if(seats[i].priority==seats[best].priority)ambiguous=true;
    }
    return ambiguous?-1:best;
}
// Identity evidence is required; a high priority or being inside the hull is insufficient.
inline bool PlayerOwnsPilotSeat(const char* name,int usable,unsigned long long occupant,unsigned long long player){
    return usable!=0 && player!=0 && occupant==player && NamedPilot(name);
}
inline bool SamePilotLease(bool confirmed,unsigned long long ship,unsigned long long seat,
                           unsigned long long ownedShip,unsigned long long ownedSeat){
    return confirmed && ship!=0 && seat!=0 && ship==ownedShip && seat==ownedSeat;
}
}
