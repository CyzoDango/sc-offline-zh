// The build built-in's menu tab: Build (objects by group, placing and the base). Moved from
// menu.cpp unchanged; registered through sco.ui by build_plugin.cpp and drawn by the menu shell.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"
#include <cstdio>
#include <cstring>


void DrawBuildTab(void*,void*){
    static int build=0,category=0;static char filter[96]="";const int n=Menu_BuildCount();
    SearchBox("##buildFilter","搜索建造对象、结构或分类",filter,sizeof(filter));if(n<=0){Hint(n<0?"正在加载建造对象……":"未找到建造对象，请检查 buildables.txt。");return;}
    if(build>=n)build=0;const int nc=Menu_BuildCategoryCount();if(category>=nc)category=0;
    ImGui::SetNextItemWidth(Ui(230));if(MBeginCombo("##category",nc?ChineseName(Name_Category,Menu_BuildCategoryName(category)).c_str():"无分类")){for(int c=0;c<nc;++c)if(V3Selectable(ChineseName(Name_Category,Menu_BuildCategoryName(c)).c_str(),c==category,c))category=c;ImGui::EndCombo();}
    ImGui::SameLine();if(MButton("放置设置"))ImGui::OpenPopup("build-options");
    float reach=Menu_BuildReach();if(MPopup("build-options")){ImGui::SetNextItemWidth(Ui(360));MSliderFloat("放置距离##reach",&reach,5,300,"%.0f 米");Hint("放在你看着的地面上，或设定距离正下方的地面。");ImGui::EndPopup();}
    const bool building=Menu_BuildModeActive();if(building){ImGui::SameLine();if(MButton("结束建造"))Menu_ToggleBuildMode();}
    bool applied=false;MListHeader();const bool searching=filter[strspn(filter," _")]!=0;int row=0;
    for(int i=0;i<n;++i){const char*name=Menu_BuildName(i);if(searching?!MatchesName(Name_Build,name,filter):Menu_BuildCategoryOf(i)!=category)continue;bool pick=false;
        const bool act=MActionRow(std::to_string(i).c_str(),ChineseName(Name_Build,name).c_str(),ChineseName(Name_Category,Menu_BuildCategory(i)).c_str(),building?"应用":"建造",build==i,&pick,row++,name);
        if(pick)build=i;if(act){build=i;Menu_SetBuild(build,reach);applied=true;if(!building){Menu_ToggleBuildMode();MenuClose();}}
    }if(!applied)Menu_SetBuild(build,reach);if(!row)Hint("没有匹配的建造对象。");
    SectionHeading("基地管理");if(MActionRow("undo","最近放置的对象","撤销一步","撤销"))Menu_BuildUndo();
    char count[48];snprintf(count,sizeof(count),"%d 个对象",Menu_BuildPlacedCount());if(MActionRow("clear","当前基地",count,"清空"))Menu_BuildClear();
    Hint("建造：鼠标左键放置，R 旋转，[ / ] 调距，退格撤销，F6 结束。");
}
