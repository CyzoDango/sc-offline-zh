#include "../src/zh/fleet_catalog.h"
#include "sco/datacore_pack.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
namespace dc=sco::datacore;
int main(int argc,char**argv){
    int checks=0,failed=0;
    auto expect=[&](bool ok,const char*why){++checks;if(!ok){++failed;std::cerr<<"FAIL "<<why<<'\n';}};
    using namespace sco_zh;
    std::map<std::string,std::vector<std::string>> groups;
    std::set<std::string> classes;
    for(const auto&h:kHullMetadata){
        expect(classes.insert(FleetLower(h.cls)).second,"unique native class");
        groups[FleetLower(ShipBase(h.cls))].push_back(h.cls);
        expect(ShipBase(h.cls).size()>0,"every class has model");
        expect(FleetRecipe(h.cls,0,-1)==h.cls,"legacy template identifier unchanged");
        if(h.ready)for(int r=0;r<4;++r)for(int c=0;c<2;++c)
            expect(!FleetRecipe(h.cls,r,c).empty()&&FleetRecipe(h.cls,r,c).size()<64,"recipe valid bounded ID");
    }
    expect(ShipBase("VNCL_Mauler_PU_AI_VAN")=="VNCL_Mauler","Mauler variants grouped");
    expect(ShipBase("RSI_Polaris_Unmanned_Salvage")=="RSI_Polaris","unmanned variants grouped");
    expect(ShipBase("DRAK_Cutlass_Black")!=ShipBase("DRAK_Cutlass_Red"),"different Cutlass models retained");
    expect(ShipBase("RSI_Constellation_Aquila_GS_AI_Template")=="RSI_Constellation_Aquila_GS","new GS templates grouped");
    expect(ShipBase("MISC_Freelancer_MAX")!=ShipBase("MISC_Freelancer_MIS"),"different freelancer models retained");
    expect(Hull("vncl_mauler")&&Hull("vncl_mauler")->length==425,"case-insensitive Mauler dimensions");
    expect(!VersionMatches("RSI_Polaris_PU_AI_CIV",1)&&VersionMatches("RSI_Polaris_PU_AI_CIV",2)&&VersionMatches("RSI_Polaris_PU_AI_CIV",4),"overlapping version filters");
    expect(FleetRecipe("unknown",3,1).empty()&&FleetRecipe("VNCL_Mauler",9,1).empty(),"unsupported recipe refused");
    for(int turn=0;turn<8;++turn){
        const double a=turn*0.3,q[4]={0,0,std::sin(a/2),std::cos(a/2)};
        FleetVec right,forward,up;ShipAxes(q,right,forward,up);
        for(int d=0;d<6;++d)for(int count=1;count<=20;++count){
            const double spacing=ShipSpacing(125,425,112,5);
            std::vector<FleetVec> points;
            for(int i=0;i<count;++i)points.push_back(ShipOffset(d,500,i,count,spacing,q));
            const auto axis=d<2?up:d<4?right:forward;
            double along=0;for(int k=0;k<3;++k)along+=points[0][k]*axis[k];
            expect(std::fabs(along-((d%2)?-500:500))<1e-6,"first hull direction and requested distance");
            for(int i=0;i<count;++i)for(int j=0;j<i;++j){double dist=0;for(int k=0;k<3;++k)dist+=std::pow(points[i][k]-points[j][k],2);expect(std::sqrt(dist)>=spacing-1e-6,"batch bounding spheres cannot overlap");}
        }
    }
    if(argc>2){
        std::ifstream in(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
        dc::Schema before;expect(before.Parse(bytes),"local database layout valid");
        if(before.recordSize){
            std::vector<dc::Pack> packs;
            for(int arg=2;arg<argc;++arg){
                std::vector<std::filesystem::path> files;
                for(const auto&e:std::filesystem::directory_iterator(std::filesystem::path(argv[arg])/"datacore"))if(e.path().extension()==".toml")files.push_back(e.path());
                std::sort(files.begin(),files.end());
                for(const auto&file:files){std::ifstream f(file);std::string text((std::istreambuf_iterator<char>(f)),{}),error;dc::Pack p;p.plugin=std::filesystem::path(argv[arg]).filename().string();p.name=file.filename().string();expect(dc::ParsePack(text,p,error),error.c_str());if(!p.ops.empty())packs.push_back(std::move(p));}
            }
            const auto result=dc::ApplyPacks(before,packs);expect(result.status.ok(),"all fleet and quantum packs emit");
            for(const auto&p:result.packs)expect(p.state==dc::PackState::Applied,p.reason.c_str());
            std::vector<uint8_t> output;expect(dc::ApplySplices(bytes,result.splices,output).ok(),"apply only to temporary memory");
            dc::Schema after;expect(after.Parse(output),"patched database layout revalidated");
            if(after.recordSize){
                dc::Patch capitalRead(after);
                auto weaponCount=[&](const char* cls,const char* weapon){
                    std::vector<dc::FieldView> fields;
                    capitalRead.ReadFields(dc::RecordRef{std::nullopt,std::string("EntityClassDefinition.")+cls},"Components[SEntityComponentDefaultLoadoutParams]",fields);
                    int count=0;const std::string expected=std::string("\"")+weapon+"\"";
                    for(const auto&f:fields)if(f.path.ends_with(".entityClassName")&&f.text==expected)++count;
                    return count;
                };
                expect(weaponCount("AEGS_Javelin","BEHR_LaserCannon_S7")==14,"original Javelin energy guns unchanged");
                expect(weaponCount("AEGS_Javelin","BEHR_JavelinBallisticCannon_S7")==12,"original Javelin ballistic guns unchanged");
                expect(weaponCount("AEGS_Javelin_PU_ZH_Ballistic","BEHR_JavelinBallisticCannon_S7")==26,"experimental Javelin has 26 existing ballistic guns");
                expect(weaponCount("AEGS_Javelin_PU_ZH_Ballistic","BEHR_LaserCannon_S7")==0,"experimental replacement applies only to its own loadout");
                // Existing vehicle/NPC/component instances must remain byte-identical.
                // The known quantum pack intentionally changes its own drive instances,
                // so only run this full preservation check on the fleet-only invocation.
                if(argc==3){
                    for(size_t i=0;i<before.mappings.size();++i){const auto&m=before.mappings[i];const size_t size=before.structInfo[m.structIndex].size*m.instanceCount;expect(!std::memcmp(bytes.data()+before.blockOffsets[i],output.data()+after.blockOffsets[i],size),"all original struct instances unchanged");}
                    for(const auto&t:before.tables)if(t.name!="data"&&t.name!="header"&&t.name!="mappings"){
                        const auto it=std::find_if(after.tables.begin(),after.tables.end(),[&](const auto&x){return x.name==t.name;});
                        if(it!=after.tables.end())expect(!std::memcmp(bytes.data()+t.offset,output.data()+it->offset,static_cast<size_t>(t.bytes)),t.name.c_str());
                    }
                }
                dc::Patch read(after);
                for(const auto&h:kHullMetadata)if(h.ready)for(int r=0;r<4;++r)for(int c=0;c<2;++c){
                    const std::string rec="EntityClassDefinition."+FleetRecipe(h.cls,r,c);
                    expect(after.FindRecordByName(rec)!=nullptr,"every UI recipe exists in emitted database");
                    std::vector<dc::FieldView> fields;
                    const auto status=read.ReadFields(dc::RecordRef{std::nullopt,rec},"Components[SEntityComponentCrewManifestParams].crewManifest",fields);
                    expect(status.ok()&&!fields.empty()&&(c?fields[0].text!="null":fields[0].text=="null"),"empty/NPC manifests set independently");
                }
                std::cout<<"Database: "<<before.records.size()<<" -> "<<after.records.size()<<" records, "<<output.size()<<" bytes\n";
            }
        }
    }
    std::cout<<"Fleet checks: "<<checks<<", failed: "<<failed<<", "<<classes.size()<<" classes in "<<groups.size()<<" model groups\n";
    return failed?1:0;
}
