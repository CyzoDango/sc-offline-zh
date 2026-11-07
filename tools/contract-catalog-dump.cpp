// Read-only export of contract IDs and script references from a user-owned local DCB.
// No CIG scripts, strings, geometry or other game assets are copied by this tool.
#include "sco/datacore.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
namespace dc=sco::datacore;
static std::string Text(std::string value){return value.size()>1&&value[0]=='"'?value.substr(1,value.size()-2):value;}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    std::ifstream f(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
    dc::Schema schema;if(!schema.Parse(bytes)){std::cerr<<schema.error;return 1;}
    dc::Patch patch(schema);
    auto ref=[&](const std::string& value){
        dc::Guid id;
        if(value.size()>50&&dc::ParseGuid(value.substr(14,36),id))
            if(const auto*record=schema.FindRecord(id))return std::string(schema.Name(record->name));
        return std::string();
    };
    for(const auto&record:schema.records){
        const std::string type(schema.StructName(record.structIndex)),name(schema.Name(record.name));
        if(type!="ContractGenerator"&&type!="ContractTemplate")continue;
        std::vector<dc::FieldView> fields;
        if(!patch.ReadFields(dc::RecordRef{std::nullopt,name},"",fields))return 1;
        std::map<std::string,std::string> values;for(const auto&field:fields)values.emplace(field.path,field.text);
        if(type=="ContractTemplate"){
            std::cout<<"T\t"<<name<<'\t';
            for(const auto&field:fields){
                const auto value=Text(field.text);
                if(value.starts_with("Libs/Subsumption/Missions/PU/")&&value.ends_with(".xml"))std::cout<<value<<'|';
                const auto target=ref(field.text);if(target.starts_with("ContractTemplate."))std::cout<<"@"<<target<<'|';
            }
            std::cout<<'\n';
        }else for(const auto&[path,value]:values){
            if(!path.ends_with(".id"))continue;
            const std::string stem=path.substr(0,path.size()-3);
            const auto debug=values.find(stem+".debugName"),templ=values.find(stem+".template");
            dc::Guid id;
            if(debug==values.end()||templ==values.end()||value.size()<45||!dc::ParseGuid(value.substr(10,36),id))continue;
            std::cout<<"G\t";for(uint8_t b:id.bytes){const char*hex="0123456789abcdef";std::cout<<hex[b>>4]<<hex[b&15];}
            std::cout<<'\t'<<Text(debug->second)<<'\t'<<ref(templ->second)<<'\t'<<name<<'\t';
            // Include nested subcontract template dependencies as well.
            for(const auto&[child,entry]:values)if(child.starts_with(stem+".")&&child.ends_with(".template")){
                const auto target=ref(entry);if(!target.empty())std::cout<<"@"<<target<<'|';
            }
            std::cout<<'\n';
        }
    }
}
