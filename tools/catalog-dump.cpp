// Read-only metadata export from a locally extracted Game2.dcb. Never packaged.
#include "sco/datacore.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
namespace dc=sco::datacore;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    dc::Schema s;if(!s.Parse(bytes)){std::cerr<<s.error;return 1;}
    dc::Patch p(s);
    std::unordered_map<std::string,std::string> guids;
    for(const auto&r:s.records)guids[dc::FormatGuid(r.id)]=s.Name(r.name);
    auto field=[&](const std::string& rec,const std::string& path){
        std::vector<dc::FieldView> f;
        if(!p.ReadFields(dc::RecordRef{std::nullopt,rec},path,f)||f.empty())return std::string();
        std::string v=f[0].text;
        if(v.starts_with("{ ref = \"guid:")) {auto it=guids.find(v.substr(14,36));if(it!=guids.end())return it->second;}
        return v;
    };
    for(const auto&r:s.records) {
        const std::string rec(s.Name(r.name)),file(s.ValueString(r.fileName));
        if(s.StructName(r.structIndex)=="EntityClassDefinition") {
            if(file.find("/entities/spaceships/")!=std::string::npos||file.find("/entities/groundvehicles/")!=std::string::npos) {
                std::cout<<"V\t"<<rec.substr(22);
                for(const char* f:{"maxBoundingBoxSize.x","maxBoundingBoxSize.y","maxBoundingBoxSize.z","vehicleSize"})
                    std::cout<<'\t'<<field(rec,std::string("Components[VehicleComponentParams].")+f);
                std::cout<<'\t'<<field(rec,"Components[SEntityComponentCrewManifestParams].crewManifest")
                         <<'\t'<<field(rec,"Components[SEntityComponentHostilityCompositeParams].emptyFactionSC")<<'\t'<<file<<'\n';
            } else if(file.find("/scitem/")!=std::string::npos)
                std::cout<<"I\t"<<rec.substr(22)<<'\t'<<file<<'\n';
            else if(file.find("/actor/")!=std::string::npos)
                std::cout<<"N\t"<<rec.substr(22)<<'\t'<<field(rec,"Components[SEntityComponentHostilityParams].factionSC")<<'\n';
        } else if(s.StructName(r.structIndex)=="CrewManifest") {
            std::vector<dc::FieldView> fields;
            p.ReadFields(dc::RecordRef{std::nullopt,rec},"crew",fields);
            for(const auto& f:fields)if(f.path.ends_with(".entityClassDefinition")) {
                auto it=guids.find(f.text.size()>50?f.text.substr(14,36):"");
                if(it!=guids.end())std::cout<<"C\t"<<rec<<'\t'<<f.path<<'\t'<<it->second<<'\n';
            }
        }
    }
}
