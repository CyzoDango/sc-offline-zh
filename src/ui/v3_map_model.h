#pragma once
#include "../zh/star_selection.h"
#include <algorithm>

namespace sco_v3 {
inline constexpr double PitchDefault=.62,PitchMin=-1.45,PitchMax=1.45;
struct Projected {double x,y,depth,factor;};
inline Projected Project(double x,double z,double yaw,double pitch,double distance){
 const double wx=x*cos(yaw)-z*sin(yaw),wz=x*sin(yaw)+z*cos(yaw);
 const double depth=wz*cos(pitch),f=distance/(std::max)(distance*.25,distance-depth);
 return {wx*f,wz*sin(pitch)*f,depth,f};
}
inline double Decay(double velocity,double dt){return velocity*pow(.92,(std::clamp)(dt,0.0,.05)*60);}
inline bool AllowedPlace(int p){return p>=0&&p<sco_map::PlaceCount&&sco_map::kBodies[sco_map::kPlaces[p].body].system==0;}
inline const char* BodyEnglish(int b){static constexpr const char*names[]={"Hurston","Arial","Aberdeen","Magda","Ita","Crusader","Cellin","Daymar","Yela","ArcCorp","Lyria","Wala","microTech","Calliope","Clio","Euterpe"};return b>=0&&b<16?names[b]:"";}
inline const char* PlaceEnglish(int p){const char*k=p>=0&&p<sco_map::PlaceCount?sco_map::kPlaces[p].key:"";
 struct Entry{const char*key,*text;};static constexpr Entry rows[]={
 {"Stanton1_Lorville","Lorville"},{"Stanton2_Orison","Orison"},{"Stanton3_Area18","Area18"},{"Stanton4_NewBabbage","New Babbage"},
 {"RR_HUR_LEO","Everus Harbor"},{"RR_CRU_LEO","Seraphim Station"},{"RR_ARC_LEO","Baijini Point"},{"RR_MIC_LEO","Port Tressler"},{"GrimHEX","GrimHEX"}};
 for(const auto&e:rows)if(std::string_view(k)==e.key)return e.text;return "";
}
inline double StationAngle(int p){const char*k=sco_map::kPlaces[p].key;const std::string_view v=k;
 return v=="RR_CRU_LEO"?1.4:v=="RR_ARC_LEO"?-1.2:v=="RR_MIC_LEO"?1.8:v=="GrimHEX"?2.2:1.;}
inline bool Experimental(int p){return AllowedPlace(p)&&sco_map::kPlaces[p].station;}
}
