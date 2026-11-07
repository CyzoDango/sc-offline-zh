#pragma once
#include "star_catalog.generated.h"
#include <cmath>
#include <string>
#include <string_view>
#include <cstddef>

namespace sco_map {
inline constexpr double Pi=3.14159265358979323846;
inline constexpr int BodyCount=int(sizeof(kBodies)/sizeof(kBodies[0]));
inline constexpr int PlaceCount=int(sizeof(kPlaces)/sizeof(kPlaces[0]));
// kPlaces 第 8~13 项（RR_HUR_LEO、RR_CRU_LEO、RR_ARC_LEO、RR_MIC_LEO、GrimHEX、RR_P3_LEO）是 zh.8 新加的空间站：
// 键名在本地 DataCore 里存在，但还没在游戏里实测过出生（实验性 / experimental, unverified in game）。
// zh.8 起启动器只接受城市和空间站；地表选点（surface=true）会被 launcher.cpp 拒绝保存和启动。
struct Vec {double x=0,y=0,z=0;};
inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator*(Vec a,double n){return {a.x*n,a.y*n,a.z*n};}
inline double Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Unit(double lat,double lon){return {cos(lat)*cos(lon),cos(lat)*sin(lon),sin(lat)};}
struct Camera {
    double lat=.35,lon=.7;
    Vec Forward()const{return Unit(lat,lon);}
    Vec Right()const{return {-sin(lon),cos(lon),0};}
    Vec Up()const{return {-sin(lat)*cos(lon),-sin(lat)*sin(lon),cos(lat)};}
    Vec Project(Vec p)const{return {Dot(p,Right()),Dot(p,Up()),Dot(p,Forward())};}
    bool Pick(double x,double y,Vec& p)const{
        const double q=1-x*x-y*y;if(q<0)return false;
        p=Right()*x+Up()*y+Forward()*sqrt(q);return true;
    }
};
struct Selection {int body=12,place=3;double lat=0,lon=0;bool surface=false;};
inline int FindBody(std::string_view key){for(int i=0;i<BodyCount;++i)if(key==kBodies[i].key)return i;return -1;}
inline int FindPlace(std::string_view key){for(int i=0;i<PlaceCount;++i)if(key==kPlaces[i].key)return i;return -1;}
inline bool Resolve(std::string_view key,double lat,double lon,Selection& out){
    if(const int p=FindPlace(key);p>=0){out={kPlaces[p].body,p,kPlaces[p].lat,kPlaces[p].lon,false};return true;}
    const int b=FindBody(key);
    if(b<0||!kBodies[b].surface||!kBodies[b].entity[0]||!std::isfinite(lat)||!std::isfinite(lon)||lat<-90||lat>90||lon<-180||lon>180)return false;
    out={b,-1,lat*Pi/180,lon*Pi/180,true};return true;
}
inline const char* Key(const Selection& s){return s.surface?kBodies[s.body].key:kPlaces[s.place].key;}
inline const char* Native(const Selection& s){return s.surface?kSystems[kBodies[s.body].system].native:kPlaces[s.place].key;}
inline const char* Name(const Selection& s){return s.surface?kBodies[s.body].name:kPlaces[s.place].name;}
inline std::string Narrow(std::wstring_view w){std::string s;for(wchar_t c:w){if(c>127)return {};s+=char(c);}return s;}
inline bool Resolve(std::wstring_view key,double lat,double lon,Selection& out){return Resolve(Narrow(key),lat,lon,out);}
inline std::wstring Wide(const char* text){std::wstring out;for(;*text;++text)out+=wchar_t(*text);return out;}
}
