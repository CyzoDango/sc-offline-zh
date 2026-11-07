#pragma once
// Metadata generated from local 4.10.2 DataCore and scene roots; no game assets.
namespace sco_map {
struct System {const char* key;const char* name;const char* native;double x,y,z;};
inline constexpr System kSystems[]={{"Stanton","史坦顿","Stanton4_NewBabbage",49.53472,-2.63396,16.47529},{"Pyro","派罗","RR_P2_L4",40.12613,8.414866,-17.2174},{"Nyx","尼克斯","Nyx_Levski",20,8.414866,-17.2174}};
struct Body {const char* key;const char* name;const char* entity;int system,parent;double radius;bool surface;int kind;unsigned a,b;double angle;};
inline constexpr Body kBodies[]={
 {"Stanton1","赫斯顿","OOC_Stanton_1_Hurston",0,-1,1000000.0,true,0,0xA99985,0xDBCEB1,0.000000000},
 {"Stanton1a","艾瑞尔","OOC_Stanton_1a_Ariel",0,0,344500.0,true,0,0xA99985,0xDBCEB1,-0.640674868},
 {"Stanton1b","阿伯丁","OOC_Stanton_1b_Aberdeen",0,0,274000.0,true,0,0xA99985,0xDBCEB1,0.637457543},
 {"Stanton1c","玛格达","OOC_Stanton_1c_Magda",0,0,340833.0,true,0,0xA99985,0xDBCEB1,-2.230615326},
 {"Stanton1d","依塔","OOC_Stanton_1d_Ita",0,0,325000.0,true,0,0xA99985,0xDBCEB1,1.745329316},
 {"Stanton2","十字军","OOC_Stanton_2_Crusader",0,-1,7450000.0,false,1,0xCEA79B,0xEEE0C5,-3.001966316},
 {"Stanton2a","赛琳","OOC_Stanton_2a_Cellin",0,5,260333.0,true,0,0xA99985,0xDBCEB1,-2.094446789},
 {"Stanton2b","戴玛尔","OOC_Stanton_2b_Daymar",0,5,295167.0,true,0,0xA99985,0xDBCEB1,1.047239253},
 {"Stanton2c","耶拉","OOC_Stanton_2c_Yela",0,5,313000.0,true,0,0xA99985,0xDBCEB1,2.443497028},
 {"Stanton3","弧光星","OOC_Stanton_3_ArcCorp",0,-1,800000.0,true,3,0x7792A1,0xC5B4A1,-0.872664641},
 {"Stanton3a","莉瑞雅","OOC_Stanton_3a_Lyria",0,9,223000.0,true,2,0xB7C8CD,0xF2F0E7,0.255351440},
 {"Stanton3b","瓦菈","OOC_Stanton_3b_Wala",0,9,283833.344,true,0,0xA99985,0xDBCEB1,2.512273980},
 {"Stanton4","微科星","OOC_Stanton_4_Microtech",0,-1,1000000.0,true,2,0x799DA6,0xF6F2E8,1.027403496},
 {"Stanton4a","卡利俄佩","OOC_Stanton_4a_Calliope",0,12,240333.0,true,2,0xB7C8CD,0xF2F0E7,0.259332561},
 {"Stanton4b","克利俄","OOC_Stanton_4b_Clio",0,12,337167.0,true,2,0xB7C8CD,0xF2F0E7,1.724166522},
 {"Stanton4c","欧忒耳佩","OOC_Stanton_4c_Euterpe",0,12,212833.0,true,2,0xB7C8CD,0xF2F0E7,1.814597363},
 {"Pyro1","派罗 I","pyro1",1,-1,411700.0,true,0,0xA99985,0xDBCEB1,-1.361354007},
 {"Pyro2","殁氧星","pyro2",1,-1,529500.0,true,0,0xA99985,0xDBCEB1,0.523601859},
 {"Pyro3","盛放星","pyro3",1,-1,413900.0,true,0,0x8D9D83,0xDECBA5,-2.094382002},
 {"Pyro4","派罗 IV","pyro4",1,20,321400.0,true,0,0xA99985,0xDBCEB1,0.504021875},
 {"Pyro5","派罗 V","pyro5",1,-1,7812300.0,false,1,0xCEA79B,0xEEE0C5,-0.174499338},
 {"Pyro6","端点星","pyro6",1,-1,285000.0,true,0,0xA99985,0xDBCEB1,-2.443430976},
 {"Pyro5a","伊格尼斯","pyro5a",1,20,393170.0,true,0,0xA99985,0xDBCEB1,-1.359240200},
 {"Pyro5b","瓦塔拉","pyro5b",1,20,365810.0,true,0,0xA99985,0xDBCEB1,2.748931622},
 {"Pyro5c","阿迪尔","pyro5c",1,20,431000.0,true,0,0xA99985,0xDBCEB1,1.784709613},
 {"Pyro5d","法伊罗","pyro5d",1,20,392240.0,true,0,0xA99985,0xDBCEB1,-0.357016854},
 {"Pyro5e","弗果","pyro5e",1,20,465670.0,true,2,0xB7C8CD,0xF2F0E7,1.010048522},
 {"Pyro5f","伏尔","pyro5f",1,20,452830.0,true,2,0xB7C8CD,0xF2F0E7,-2.327605463},
 {"Nyx1","尼克斯 I","",2,-1,1000000.0,false,0,0xA99985,0xDBCEB1,0.000000000},
 {"Nyx2","尼克斯 II","",2,-1,1000000.0,false,0,0xA99985,0xDBCEB1,0.000000000},
 {"Nyx3","尼克斯 III","",2,-1,1000000.0,false,1,0xCEA79B,0xEEE0C5,0.000000000},
 {"Delamar","德拉玛","",2,-1,75000.0,false,0,0xA99985,0xDBCEB1,0.000000000},
};
struct Place {const char* key;const char* name;int body;double lat,lon;bool station;};
inline constexpr Place kPlaces[]={
 {"Stanton1_Lorville","罗威尔",0,0.608492009122,-1.982982553297,false},
 {"Stanton2_Orison","奥里森",5,0.777587135035,-0.161583467301,false},
 {"Stanton3_Area18","18 区",9,-0.333526852293,-2.986660116636,false},
 {"Stanton4_NewBabbage","新巴贝奇",12,0.838497450894,0.677994694440,false},
 {"Nyx_Levski","列夫斯基",31,0.314550815126,-0.902972674986,false},
 {"RR_P2_L4","死局空间站 (Checkmate)",17,0.400000000000,0.800000000000,true},
 {"RR_P6_LEO","废墟空间站",21,0.400000000000,0.800000000000,true},
 {"RR_HUR_LEO","埃弗勒斯空间站",0,0.400000000000,0.800000000000,true},
 {"RR_CRU_LEO","炽天使空间站",5,0.400000000000,0.800000000000,true},
 {"RR_ARC_LEO","拜基尼空间站",9,0.400000000000,0.800000000000,true},
 {"RR_MIC_LEO","特雷斯勒空间站",12,0.400000000000,0.800000000000,true},
 {"GrimHEX","六角湾",8,0.400000000000,0.800000000000,true},
 {"RR_P3_LEO","轨道讣闻站",18,0.400000000000,0.800000000000,true},
};
}
