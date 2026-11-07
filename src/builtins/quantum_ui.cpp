// The quantum built-in's menu tab: Travel (places, the scan and saved spots). Moved from menu.cpp
// unchanged; registered through sco.ui by quantum_plugin.cpp and drawn by the menu shell.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"
#include "../travel.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static bool HasText(const char* filter) { return filter[strspn(filter, " _")] != 0; }


void DrawTravelTab(void*,void*){
    static TravelPlace places[3000];static TravelBookmark marks[400];static int order[3000],np=0,placesVersion=-1,system=0,view=0;
    static char filter[96]="",markName[64]="",selected[96]="";static float altitude=2000;static bool minor=false;
    const int version=Travel_PlacesVersion();if(version!=placesVersion){placesVersion=version;np=Travel_GetPlaces(places,3000);for(int i=0;i<np;++i)order[i]=i;std::sort(order,order+np,[&](int a,int b){return _stricmp(places[a].entity,places[b].entity)<0;});}
    const int nm=Travel_GetBookmarks(marks,400);char here[32];Travel_CurrentSystem(here,sizeof(here));
    std::vector<std::string>systems={"全部星系"};auto add=[&](const char*t){if(t[0]&&std::find(systems.begin()+1,systems.end(),t)==systems.end())systems.push_back(t);};
    add(here);for(int i=0;i<np;++i)add(places[i].system);for(int i=0;i<nm;++i)add(marks[i].system);system=std::clamp(system,0,int(systems.size())-1);
    SearchBox("##travelFilter","搜索地点、星系或已保存的位置",filter,sizeof(filter));
    if(MChip("地点",view==0))view=0;ImGui::SameLine();if(MChip("已保存",view==1))view=1;ImGui::SameLine();
    ImGui::SetNextItemWidth(Ui(150));if(MBeginCombo("##system",system?ChineseName(Name_System,systems[system].c_str()).c_str():"全部星系")){for(int i=0;i<int(systems.size());++i)if(V3Selectable(i?ChineseName(Name_System,systems[i].c_str()).c_str():"全部星系",i==system,i))system=i;ImGui::EndCombo();}
    ImGui::SameLine();if(MButton("到达设置"))ImGui::OpenPopup("arrival-options");ImGui::SameLine();
    float progress=0;if(!Travel_Scanning(progress)){if(MButton("扫描"))Travel_RequestScan();}else ImGui::Text("扫描 %.0f%%",progress*100);
    if(MPopup("arrival-options")){ImGui::SetNextItemWidth(Ui(400));MSliderFloat("到达高度##altitude",&altitude,100,20000,"%.0f 米",ImGuiSliderFlags_Logarithmic);V3Toggle("显示室内和小型区域",&minor);Hint("包括电梯厅、机库、小行星带分区。只能传送到当前星系里已加载的地方。");ImGui::EndPopup();}
    if(view==0){MListHeader();int row=0;static const char*kinds[]={"行星","卫星","地点","室内"};
        for(int k=0;k<np;++k){const auto&p=places[order[k]];if(system&&_stricmp(systems[system].c_str(),p.system))continue;if(p.kind==Place_Minor&&!minor)continue;
            const auto name=ChinesePlace(p.name,p.entity);if(!MatchesName(Name_Place,p.name,filter)&&!MatchesFilter(name.c_str(),filter)&&!MatchesName(Name_System,p.system,filter)&&!MatchesFilter(p.entity,filter))continue;
            const std::string kind=ChineseName(Name_System,p.system)+" · "+kinds[p.kind>=0&&p.kind<=3?p.kind:2];bool pick=false;
            const bool go=MActionRow(p.entity,name.c_str(),kind.c_str(),"传送",!_stricmp(selected,p.entity),&pick,row++,p.entity);if(pick)strcpy_s(selected,p.entity);if(go||(pick&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))){strcpy_s(selected,p.entity);Travel_RequestPlace(p,altitude);}
        }if(!row)Hint("没找到已加载的地点。换个筛选条件，或者点“扫描”。");
    }else{
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Ui(125));MTextField("##markName","为当前位置命名",markName,sizeof(markName));ImGui::SameLine();if(MButton("保存当前位置")){Travel_RequestSaveBookmark(markName);markName[0]=0;}
        MListHeader();static int chosen=-1;int row=0;for(int i=0;i<nm;++i){const auto&m=marks[i];if((system&&_stricmp(m.system,systems[system].c_str()))||!MatchesFilter(m.name,filter))continue;bool pick=false;
            if(MActionRow(std::to_string(i).c_str(),m.name,ChineseName(Name_System,m.system).c_str(),"传送",chosen==i,&pick,row++))Travel_RequestBookmark(i);if(pick)chosen=i;
            if(chosen==i){if(MButton("删除此位置",true)){Travel_RequestDeleteBookmark(i);chosen=-1;}}
        }if(!row)Hint("没有匹配的已保存位置。");
    }Hint("F7 保存，F8 返回。扫描只读不改；只能在当前星系里传送。");
}
