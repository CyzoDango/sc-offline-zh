#include "tabs.h"
#include "../contracts.h"
#include "../missions.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"


void DrawContractsTab(void*,void*){
    static char filter[96]="";SearchBox("##contractSearch","搜索任务类型",filter,sizeof(filter));
    const auto state=ReadContractsStatus();int scripts=0;bool registered=false;ReadMissionScriptStatus(scripts,registered);
    const char*categories[]={"全部支持的类型","货运 / 交付","空战 / 赏金 / 巡逻","地面 / 清剿 / 狩猎","调查 / 打捞 / 其他"};
    MListHeader("名称","候选数量");for(int i=0;i<5;++i){if(!MatchesFilter(categories[i],filter))continue;char count[48];snprintf(count,sizeof(count),"%d 个候选",state.categories[i]);
        ImGui::BeginDisabled(!state.known||!state.categories[i]);if(MActionRow(std::to_string(i).c_str(),categories[i],count,"列出",false,nullptr,i))RequestContractOffers(i);ImGui::EndDisabled();}
    SectionHeading("任务状态");char text[128];snprintf(text,sizeof(text),"%d / %d",state.supported,state.known);MActionRow("definitions","满足脚本条件 / 已读取合约",text,nullptr);
    snprintf(text,sizeof(text),"%d / %d / %d",state.listed,state.running,state.starting);MActionRow("offers","已列出 / 运行中 / 创建中",text,nullptr);
    snprintf(text,sizeof(text),"%d 个 · %s",scripts,registered?"已提交":"未就绪");MActionRow("scripts","本地脚本",text,nullptr);
    Hint("点“列出”会把任务发到 mobiGlas，接取和执行都在 mobiGlas 里。缺脚本的任务不会出现。");
}
