// Grouped hull browser, independent offline recipes, paced batches and session ownership.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static std::string ShortShipName(const char* cls){
    std::string name=ChineseShipLabel(cls);const size_t at=name.find("  |  ");if(at!=std::string::npos)name.resize(at);return name;
}
static bool ModelVisible(int model,int version,const char* filter){
    for(int v=0;v<ShipModelVariantCount(model);++v){
        const int i=ShipModelVariant(model,v);
        if(ShipVersionMatches(Menu_Ships()[i].name,version)&&(MatchesShipFilter(ShipModelName(model),filter)||MatchesShipFilter(Menu_Ships()[i].name,filter)))return true;
    }return false;
}

static void DrawOwned(bool items){
    MenuSpawnedShip list[512];const int n=Menu_GetSpawnedShips(list,512,items);MListHeader();int active=0;
    for(int i=n-1;i>=0;--i){const auto&v=list[i];if(v.state==3)continue;++active;
        const std::string name=items?ChineseName(Name_Item,v.cls):ShortShipName(v.cls),id=std::to_string(v.id);
        ImGui::BeginDisabled(v.state==2);if(MActionRow(id.c_str(),name.c_str(),v.state==2?"正在删除":v.state==0?"正在载入":v.state==4?"未载入":items?"物品":"舰船","删除",false,nullptr,active-1,v.cls))Menu_RequestDeleteShip(v.id);ImGui::EndDisabled();
    }
    if(!active)Hint(items?"这次还没生成物品。":"这次还没生成飞船。");
    ImGui::BeginDisabled(!active);if(MActionRow("clearOwned",items?"清理本次生成的物品":"清理本次生成的飞船",items?"全部物品":"保留所在飞船","全部删除"))Menu_RequestDeleteAllShips(items);ImGui::EndDisabled();
}
static void DrawBatchProgress(){
    int done=0,total=0;if(!Menu_SpawnBatchBusy(&done,&total))return;
    char text[64];snprintf(text,sizeof(text),"生成队列 %d / %d",done,total);
    if(MActionRow("batch",text,"后续排队项目","停止"))Menu_RequestCancelBatch();
}
static void ShipSettings(){
    SectionHeading("当前飞船");MenuPilotContext pilot;const bool confirmed=Menu_GetCurrentPilot(pilot);
    if(confirmed){const std::string name=ShortShipName(pilot.shipName);ImGui::Text("主驾驶已确认 · %s",name.c_str());}
    else Hint("先坐进主驾驶座。站在船里、坐副驾驶或炮塔都用不了这些功能。");
    bool ammo=Menu_CurrentShipAmmoOn();ImGui::BeginDisabled((!confirmed&&!ammo)||!Menu_CurrentShipAmmoKnown());
    if(MSetting("飞船无限弹药","只在主驾驶座生效，离座或换船自动关闭",&ammo))Menu_RequestCurrentShipAmmo(ammo);ImGui::EndDisabled();
    static constexpr int order[]={Ship_InfinitePower,Ship_NoOverheat,Ship_AIGunnerAmmo};
    for(int i:order){bool on=Menu_ShipSettingOn(i);const bool known=Menu_ShipSettingKnown(i);ImGui::BeginDisabled(!known||(!confirmed&&!on));
        const char*label=i==Ship_InfinitePower?"无限电力":i==Ship_NoOverheat?"不过热":"AI 炮手无限弹药";
        const char*tip=!known?"当前游戏版本没有这个选项":i==Ship_InfinitePower?"改的是整个场景的资源规则，离开主驾驶座后恢复":i==Ship_NoOverheat?"改的是整个场景的过热规则，离开主驾驶座后恢复":"改的是整个场景的 AI 弹药规则，离开主驾驶座后恢复";
        if(MSetting(label,tip,&on))Menu_RequestShipSetting(i,on);ImGui::EndDisabled();
    }
    ImGui::BeginDisabled(!confirmed);
    if(MActionRow("power","当前飞船电源","实际所在的主驾驶座","切换"))Menu_RequestCurrentFlightReady();ImGui::EndDisabled();
    Hint("速度和加速度保持游戏原样，暂时不能单独调某艘船的速度。");
}
static void SpawnOptions(MenuSpawnOptions&opt,int selected){
    const char*directions[]={"上方","下方","右方","左方","前方","后方"};ImGui::SetNextItemWidth(Ui(360));MCombo("##direction",&opt.direction,directions,6);
    MSliderFloat("生成距离##distance",&opt.height,1,10000,"%.0f 米",ImGuiSliderFlags_Logarithmic);
    MSliderInt("生成数量##quantity",&opt.count,1,20,"%d 艘");
    char spacing[96];snprintf(spacing,sizeof(spacing),"中心间距 %.0f 米",selected>=0?Menu_ShipSpacing(selected,opt.extraGap):0);Hint(spacing);
    MSliderFloat("额外间隔##gap",&opt.extraGap,0,5000,"%.0f 米");
    const char*boards[]={"不登船","进入第一艘驾驶座","按座位名称进入第一艘","生成后自行选择座位"};ImGui::SetNextItemWidth(-1);MCombo("##board",&opt.seatMode,boards,4);
    if(opt.seatMode==SeatMode_Named){ImGui::SetNextItemWidth(-1);MTextField("##seatName","座位英文原名关键词，如 copilot",opt.seatName,sizeof(opt.seatName));}
    const bool boarding=opt.seatMode==SeatMode_Pilot||opt.seatMode==SeatMode_Named;ImGui::BeginDisabled(!boarding);
    V3Toggle("移除我座位上的 NPC",&opt.replaceNpc);V3Toggle("登船后通电",&opt.flightReady);ImGui::EndDisabled();
    Hint("方向以点“生成”时你面朝的方向为准。大船最好离远一点；多艘船会按船体大小自动排开。");
}
void DrawVehiclesTab(void*,void*){
    static int model=0,variant=0,version=0,sort=0,view=0;static char filter[96]="";
    static MenuSpawnOptions opt=[]{MenuSpawnOptions o;o.crew=0;o.height=200;return o;}();
    SearchBox("##shipFilter","搜索中文船名、英文型号或版本关键词",filter,sizeof(filter));
    const char*views[]={"舰船目录","已生成","当前飞船"};for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(MChip(views[i],view==i))view=i;}
    if(view==1){DrawOwned(false);DrawBatchProgress();return;}if(view==2){ShipSettings();return;}
    const int count=ShipModelCount();if(count<=0){Hint(count<0?"正在加载船型，请先进入游戏场景……":"未找到飞船，请检查 data/ships.txt。");return;}
    const auto*ships=Menu_Ships();const char*versions[]={"全部版本","原版","PU 版本","EA／AC 版本","AI 版本","无人／残骸／测试"};
    const char*sorts[]={"长度↓","长度↑","名称排序"};const char*crews[]={"空船","NPC 船员"};
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{Ui(8),Ui(6.5f)});
    const float col=(ImGui::GetContentRegionAvail().x-Ui(76))/3;
    ImGui::SetNextItemWidth(col);if(MCombo("##versionFilter",&version,versions,6))variant=0;
    ImGui::SameLine();ImGui::SetNextItemWidth(col);MCombo("##shipSort",&sort,sorts,3);
    ImGui::SameLine();ImGui::SetNextItemWidth(col);MCombo("##crew",&opt.crew,crews,2);ImGui::SameLine();
    if(MButton("设置"))ImGui::OpenPopup("spawn-options");ImGui::PopStyleVar();
    std::vector<int>visible;for(int m=0;m<count;++m)if(ModelVisible(m,version,filter))visible.push_back(m);
    std::stable_sort(visible.begin(),visible.end(),[&](int a,int b){if(sort==2)return ShortShipName(ShipModelName(a))<ShortShipName(ShipModelName(b));const float x=ships[ShipModelVariant(a,0)].length,y=ships[ShipModelVariant(b,0)].length;return sort==0?x>y:x<y;});
    if(std::find(visible.begin(),visible.end(),model)==visible.end()){model=visible.empty()?-1:visible.front();variant=0;}
    std::vector<int>variants;auto variantsOf=[&](int m){std::vector<int>out;for(int v=0;v<ShipModelVariantCount(m);++v){int i=ShipModelVariant(m,v);if(ShipVersionMatches(ships[i].name,version))out.push_back(i);}return out;};
    if(model>=0)variants=variantsOf(model);if(variant>=int(variants.size()))variant=0;
    int selected=variants.empty()?-1:variants[variant];
    if(selected>=0){ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{Ui(10),Ui(6.5f)});ImGui::SetNextItemWidth(-1);
        const std::string title="所选版本 · "+ShortShipName(ShipModelName(model))+" / "+ShipVariantName(ships[selected].name);
        if(MBeginCombo("##shipVariant",title.c_str())){for(int v=0;v<int(variants.size());++v){ImGui::PushID(v);if(V3Selectable(ShipVariantName(ships[variants[v]].name).c_str(),v==variant,v))variant=v;ImGui::PopID();}ImGui::EndCombo();}ImGui::PopStyleVar();selected=variants[variant];
    }
    if(MPopup("spawn-options")){SectionHeading("生成设置");SpawnOptions(opt,selected);ImGui::EndPopup();}
    char catalogStatus[96];snprintf(catalogStatus,sizeof(catalogStatus),"%d 个船型 · 已注册 %d 个载具类",int(visible.size()),Menu_ShipCount());Hint(catalogStatus);
    MListHeader();const bool busy=Menu_SpawnBatchBusy();int row=0;
    for(int m:visible){const auto list=variantsOf(m);if(list.empty())continue;const int index=m==model?list[(std::min)(variant,int(list.size())-1)]:list[0];const auto&v=ships[index];
        char kind[96];if(v.length>0)snprintf(kind,sizeof(kind),"舰船 · S%d / %.0f米",v.size,v.length);else snprintf(kind,sizeof(kind),"舰船 · S%d",v.size);
        const bool recipe=Menu_FleetRecipeAvailable(index,opt.relation,opt.crew);bool pick=false;ImGui::BeginDisabled(busy);
        const bool spawn=MActionRow(std::to_string(m).c_str(),ShortShipName(ShipModelName(m)).c_str(),kind,recipe?"生成":"原模板",m==model,&pick,row++,v.name);ImGui::EndDisabled();
        if(pick&&model!=m){model=m;variant=0;}
        if(spawn){if(model!=m)variant=0;model=m;auto send=opt;const bool boarding=send.seatMode==SeatMode_Pilot||send.seatMode==SeatMode_Named;if(!recipe){send.relation=0;send.crew=-1;}else if(!boarding)send.flightReady=false;Menu_RequestSpawn(index,send);}
    }
    if(visible.empty())Hint("没有匹配的船型，换个名称或版本试试。");DrawBatchProgress();
    Hint("点船名选版本。“设置”里可以调方向、距离、数量、间距和登船方式；“原模板”会保留原本的阵营关系和船员。");
}
void DrawItemsTab(void*,void*){
    static char filter[96]="";static int selected=-1,category=0,quantity=1,view=0;static float distance=3;
    SearchBox("##itemSearch","搜索中文物品名、英文名或原始型号",filter,sizeof(filter));
    if(MChip("物品目录",view==0))view=0;ImGui::SameLine();if(MChip("已生成",view==1))view=1;
    if(view==1){DrawOwned(true);DrawBatchProgress();return;}
    const int n=Menu_SpawnItemCount();if(n<0){Hint("正在读取本机游戏中的物品……");return;}
    static std::vector<std::string>cats;static int last=-1;if(last!=n){cats={"全部分类"};for(int i=0;i<n;++i){std::string c=Menu_SpawnItemCategory(i);if(std::find(cats.begin(),cats.end(),c)==cats.end())cats.push_back(c);}last=n;}
    category=std::clamp(category,0,int(cats.size())-1);ImGui::SetNextItemWidth(Ui(200));
    if(MBeginCombo("##itemCategory",cats[category].c_str())){for(int c=0;c<int(cats.size());++c)if(V3Selectable(cats[c].c_str(),c==category,c))category=c;ImGui::EndCombo();}
    ImGui::SameLine();if(MButton("生成设置"))ImGui::OpenPopup("item-options");
    if(MPopup("item-options")){ImGui::SetNextItemWidth(Ui(360));MSliderInt("生成数量##itemQty",&quantity,1,20,"%d 件");MSliderFloat("人物前方##itemDistance",&distance,1,100,"%.1f 米");ImGui::EndPopup();}
    MListHeader();int row=0;for(int i=0;i<n;++i){const char*cls=Menu_SpawnItemName(i);if((category&&cats[category]!=Menu_SpawnItemCategory(i))||!MatchesName(Name_Item,cls,filter))continue;
        bool pick=false;ImGui::BeginDisabled(Menu_SpawnBatchBusy());const bool spawn=MActionRow(std::to_string(i).c_str(),ChineseName(Name_Item,cls).c_str(),Menu_SpawnItemCategory(i),"生成",selected==i,&pick,row++,cls);ImGui::EndDisabled();if(pick)selected=i;
        if(spawn){selected=i;Menu_RequestSpawnItem(selected,quantity,distance);}
    }if(!row)Hint("没有匹配的物品。");DrawBatchProgress();Hint("物品会生成在你面前。要穿装备去“玩家”页。");
}
