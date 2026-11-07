#include "menu.h"
#include "menu_ui.h"
#include "zh/fleet_catalog.h"
#include <unordered_map>
namespace {
struct Model {std::string cls;std::vector<int> variants;};
std::vector<Model> models;
int lastCount=-1;
void Build(){
    const int n=Menu_ShipCount();if(n<0||n==lastCount)return;
    models.clear();std::unordered_map<std::string,size_t> groups;
    const auto* ships=Menu_Ships();
    for(int i=0;i<n;++i){
        const std::string base=sco_zh::ShipBase(ships[i].name),key=sco_zh::FleetLower(base);
        auto [it,fresh]=groups.emplace(key,models.size());
        if(fresh)models.push_back({base,{}});
        models[it->second].variants.push_back(i);
    }
    for(auto&m:models)std::stable_sort(m.variants.begin(),m.variants.end(),[&](int a,int b){
        const bool aa=sco_zh::VersionMatches(ships[a].name,1),bb=sco_zh::VersionMatches(ships[b].name,1);
        return aa!=bb?aa:sco_zh::FleetLower(ships[a].name)<sco_zh::FleetLower(ships[b].name);
    });
    lastCount=n;
}
}
int ShipModelCount(){Build();return Menu_ShipCount()<0?-1:static_cast<int>(models.size());}
const char* ShipModelName(int i){return i>=0&&i<static_cast<int>(models.size())?models[i].cls.c_str():"";}
int ShipModelVariantCount(int i){return i>=0&&i<static_cast<int>(models.size())?static_cast<int>(models[i].variants.size()):0;}
int ShipModelVariant(int i,int v){return i>=0&&i<static_cast<int>(models.size())&&v>=0&&v<static_cast<int>(models[i].variants.size())?models[i].variants[v]:-1;}
std::string ShipVariantName(const char* c){return sco_zh::VariantName(c);}
bool ShipVersionMatches(const char* c,int f){return sco_zh::VersionMatches(c,f);}
