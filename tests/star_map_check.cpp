#include "../src/zh/star_selection.h"
#include "../src/zh/start_location.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

int main(){
    using namespace sco_map;
    int checks=0,failures=0;
    auto expect=[&](bool ok,const char* label){++checks;if(!ok){++failures;printf("FAIL %s\n",label);}};
    auto close=[](double a,double b){return fabs(a-b)<1e-10;};
    std::set<std::string> keys;
    for(int b=0;b<BodyCount;++b){
        const auto&body=kBodies[b];
        expect(keys.insert(body.key).second,"unique body key");
        expect(body.system>=0&&body.system<3&&body.radius>100,"system and physical radius");
        expect(body.parent<0||(body.parent<BodyCount&&body.parent!=b&&kBodies[body.parent].system==body.system),"moon belongs to its own system");
        Selection selected;const bool accepted=Resolve(std::string_view(body.key),23.5,-67.25,selected);
        expect(accepted==(body.surface&&body.entity[0]),"unsupported bodies cannot become ground destinations");
        if(accepted){
            expect(selected.surface&&selected.body==b&&close(selected.lat,23.5*Pi/180)&&close(selected.lon,-67.25*Pi/180),"ground coordinates retain degrees and hemisphere");
            expect(!strcmp(Native(selected),kSystems[body.system].native),"surface destination uses an anchor in the same system");
        }
    }
    for(int p=0;p<PlaceCount;++p){
        const auto&place=kPlaces[p];Selection selected;
        expect(keys.insert(place.key).second,"unique native place key");
        expect(place.body>=0&&place.body<BodyCount&&fabs(place.lat)<=Pi/2&&fabs(place.lon)<=Pi,"native marker is on a valid body");
        expect(Resolve(std::string_view(place.key),0,0,selected)&&selected.place==p&&!selected.surface,"native marker retains native mode");
        expect(!strcmp(Native(selected),place.key),"native marker is not replaced by a system anchor");
        expect(sco_start::NativeArguments(L"StarMap",Wide(place.key))==L" +sv_OverridePlayerPersistSpawnLocation "+Wide(place.key),"native start argument matches selected place");
    }
    Selection selected;
    expect(!Resolve(std::string_view("Stanton4 +quit"),0,0,selected),"command text is not a catalog key");
    expect(!Resolve(std::wstring_view(L"新巴贝奇"),0,0,selected),"translated text cannot replace a native identifier");
    expect(!Resolve(std::string_view("Stanton4"),90.01,0,selected),"latitude outside the globe refused");
    expect(!Resolve(std::string_view("Stanton4"),0,-180.01,selected),"longitude outside the globe refused");
    expect(!Resolve(std::string_view("Stanton4"),std::numeric_limits<double>::quiet_NaN(),0,selected),"NaN refused");
    expect(!Resolve(std::string_view("Stanton4"),0,std::numeric_limits<double>::infinity(),selected),"infinity refused");
    expect(Resolve(std::string_view("Stanton4"),-90,180,selected),"poles and date line accepted");
    expect(sco_start::NativeArguments(L"NewBabbage")==L" +sv_OverridePlayerPersistSpawnLocation Stanton4_NewBabbage","legacy New Babbage setting retained");
    expect(sco_start::NativeArguments(L"StarMap",L"unknown").empty(),"unknown target produces no game command");
    for(double lat:{-1.35,0.0,.84,1.35})for(double lon:{-3.0,0.0,2.8}){
        Camera cam{lat,lon};Vec center;
        expect(close(Dot(cam.Right(),cam.Up()),0)&&close(Dot(cam.Forward(),cam.Up()),0),"camera basis remains orthogonal after rotation");
        expect(cam.Pick(0,0,center)&&close(Dot(center,cam.Forward()),1),"globe center selects the visible hemisphere");
        for(double x:{-.7,0.0,.5})for(double y:{-.3,0.0,.4}){
            Vec hit;const bool picked=cam.Pick(x,y,hit);const Vec screen=cam.Project(hit);
            expect(picked&&close(Dot(hit,hit),1)&&close(screen.x,x)&&close(screen.y,y)&&screen.z>=0,"screen picking round trips after arbitrary rotation");
        }
        Vec ignored;expect(!cam.Pick(1.1,0,ignored),"click outside the sphere is rejected");
    }
    printf("star map: %d checks, %d failures; %d bodies, %d native places; no game or disk writes\n",checks,failures,BodyCount,PlaceCount);
    return failures?1:0;
}
