// The npc built-in's menu tab: NPCs, and the NPC picker the Crew tab shares. Registered through
// sco.ui (RegisterNpcsTab, called by npc_plugin.cpp) and drawn by the menu shell. The buttons run
// the built-in's npc.spawn and npc.clear commands through sco_api's invoke.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"
#include <cstdio>
#include <cstring>

static const sco_api* g_api = nullptr;
static sco_plugin*    g_self = nullptr;

// The command's reply (a refusal, or "Spawning ...") in the menu's status strip, which also logs it.
static void ShowReply(sco_result, const char* reply, void*) { SetMenuStatus("%s", reply ? reply : ""); }

static void InvokeNpcCommand(const char* name, const sco_arg* args, uint32_t nargs) {
    if (g_api && g_self) g_api->invoke(g_self, name, args, nargs, ShowReply, nullptr);
}

void RegisterNpcsTab(const sco_api* api, sco_plugin* self) {
    g_api = api;
    g_self = self;
    RegisterBuiltinTab(api, self, "npc.npcs", "角色", kTabNpcs, DrawNpcsTab);
}

// --- NPC picker, shared by the NPCs and Crew tabs -------------------------------------------

int g_npcPick = 0;

bool NpcPicker() {
    static char filter[64] = "";
    const int npcs = Menu_NpcCount();
    if (npcs < 0) { Hint("正在加载 NPC（需要先进入游戏场景）……"); return false; }
    if (npcs == 0) { Hint("未找到 NPC，请检查 data\\npcs.txt。"); return false; }
    if (g_npcPick >= npcs) g_npcPick = 0;
    if (SearchBox("##npcFilter", "搜索 NPC", filter, sizeof(filter)))
        for (int i = 0; i < npcs; ++i)
            if (MatchesName(Name_Npc, Menu_NpcName(i), filter)) { g_npcPick = i; break; }
    const auto& preview = ChineseName(Name_Npc, Menu_NpcName(g_npcPick));
    ImGui::SetNextItemWidth(-1);
    if (MBeginCombo("##npc", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < npcs; ++i) {
            const char* name = Menu_NpcName(i);
            if (!MatchesName(Name_Npc, name, filter)) continue;
            const std::string label = ChineseName(Name_Npc, name) + "##n" + std::to_string(i);
            if (V3Selectable(label.c_str(), i == g_npcPick, i)) g_npcPick = i;
            ImGui::SetItemTooltip("%s", name);
            if (i == g_npcPick) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    const bool found = MatchesName(Name_Npc, Menu_NpcName(g_npcPick), filter);
    if (!found) Hint("没有匹配的角色，请换一个名称或清空搜索。");
    return found;
}


void DrawNpcsTab(void*,void*){
    static char filter[96]="";static int quantity=1;SearchBox("##npcSearch","搜索角色、阵营或 NPC 型号",filter,sizeof(filter));
    if(MButton("生成数量"))ImGui::OpenPopup("npc-options");ImGui::SameLine();if(MButton("移除已生成 NPC",true)){InvokeNpcCommand("npc.clear",nullptr,0);Menu_RequestClearNpcs();}
    if(MPopup("npc-options")){ImGui::SetNextItemWidth(Ui(360));MSliderInt("数量##howMany",&quantity,1,10,"%d 名");ImGui::EndPopup();}
    const int n=Menu_NpcCount();if(n<=0){Hint(n<0?"正在加载 NPC……":"未找到 NPC，请检查 npcs.txt。");return;}MListHeader();int row=0;
    for(int i=0;i<n;++i){const char*name=Menu_NpcName(i);if(!MatchesName(Name_Npc,name,filter))continue;bool pick=false;
        const bool fire=MActionRow(std::to_string(i).c_str(),ChineseName(Name_Npc,name).c_str(),"NPC 角色","生成",g_npcPick==i,&pick,row++,name);if(pick)g_npcPick=i;
        if(fire){g_npcPick=i;sco_arg args[2]={};args[0].type=SCO_ARG_STRING;args[0].v.s=Menu_NpcName(g_npcPick);args[1].type=SCO_ARG_INT;args[1].v.i=quantity;InvokeNpcCommand("npc.spawn",args,2);MenuClose();}
    }if(!row)Hint("没有匹配的角色。“移除已生成 NPC”也会移除这个菜单生成的船员。");
}
