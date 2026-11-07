// The crew built-in's menu tab: Crew (the target ship's seats and its NPC crew). Moved from
// menu.cpp unchanged; registered through sco.ui by crew_plugin.cpp and drawn by the menu shell.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../spawner.h"
#include "../teleport.h"
#include "../third_party/imgui/imgui.h"
#include <cstdio>
#include <cstring>

static std::string CrewShipLabel(const char* cls){
    std::string label=ChineseShipLabel(cls);const auto at=label.find("  |  ");
    if(at!=std::string::npos)label.resize(at);return label;
}

// "VNCL_Mauler_Gunner_Seat_200006242347" on a VNCL_Mauler -> "Gunner Seat 3"
static void PrettySeatNames(const MenuSeat* seats, int n, const char* ship, char (*out)[64]) {
    static char base[128][64];
    for (int i = 0; i < n; ++i) {
        char name[64];
        strcpy_s(name, seats[i].name);
        if (char* cut = strrchr(name, '_'); cut && cut[1] && strspn(cut + 1, "0123456789") == strlen(cut + 1)) *cut = 0;
        const char* a = name;          // drop the leading words the seat shares with the ship's name
        const char* b = ship;
        for (;;) {
            const size_t la = strcspn(a, "_"), lb = strcspn(b, "_ ");
            if (!la || la != lb || _strnicmp(a, b, la) != 0 || !a[la]) break;
            a += la + 1;
            b += lb + (b[lb] ? 1 : 0);
        }
        strcpy_s(base[i], a);
        for (char* c = base[i]; *c; ++c) if (*c == '_') *c = ' ';
    }
    for (int i = 0; i < n; ++i) {      // number repeats: Gunner Seat 1, Gunner Seat 2, ...
        int total = 0, before = 0;
        for (int j = 0; j < n; ++j)
            if (_stricmp(base[i], base[j]) == 0) { ++total; if (j < i) ++before; }
        if (total > 1) snprintf(out[i], 64, "%s %d", base[i], before + 1);
        else strcpy_s(out[i], 64, base[i]);
    }
}

// Run by the crew built-in's tick: the tab's buttons queue spawner.cpp's seat actions, which this carries out.
void CrewTabTick(uint32_t now) {
    if (g_tp.ok) ProcessCrew(now);
}


void DrawCrewTab(void*,void*){
    static char filter[96]="";SearchBox("##seatSearch","搜索目标船的座位",filter,sizeof(filter));
    if(!Menu_SeatControlAvailable()){Hint("当前游戏版本的座位控制不可用，详情见 mod.log。");return;}
    static MenuSeat seats[128];static char pretty[128][64];static unsigned long long selectedSeat=0,previous=0;static bool replace=true;
    char ship[64]="";const int count=Menu_GetSeats(seats,128,ship,sizeof(ship));const auto target=Menu_CrewTargetShip();if(target!=previous){selectedSeat=0;previous=target;}
    static MenuSpawnedShip fleet[512];const int total=Menu_GetSpawnedShips(fleet,512);int available=0;for(int i=0;i<total;++i)if(fleet[i].state<2)++available;
    ImGui::SetNextItemWidth(Ui(330));const auto title=count<0?std::string("从已生成舰船选择"):CrewShipLabel(ship);
    if(MBeginCombo("##crew-target",title.c_str())){for(int i=0;i<total;++i){const auto&row=fleet[i];if(row.state==2||row.state==3)continue;
        const std::string label=CrewShipLabel(row.cls)+" · #"+std::to_string(row.id);ImGui::PushID(i);ImGui::BeginDisabled(row.state==4);
        if(V3Selectable(label.c_str(),row.id==target,i)){selectedSeat=0;Menu_TargetSpawnedShip(row.id);}ImGui::EndDisabled();ImGui::PopID();
    }if(!available)Hint("这次还没有可选的飞船");ImGui::EndCombo();}
    ImGui::SameLine();if(MButton("使用所在飞船"))Menu_TargetShipImIn();ImGui::SameLine();if(MButton("船员设置"))ImGui::OpenPopup("crew-options");
    ImGui::SetNextWindowSizeConstraints({Ui(420),0},{Ui(520),Ui(440)});
    if(MPopup("crew-options")){V3Toggle("入座时替换 NPC",&replace);const bool have=NpcPicker();int filled=0,totalFill=0;const bool busy=Menu_CrewFillBusy(&filled,&totalFill);
        ImGui::BeginDisabled(count<=0||!have||busy);if(MButton("生成驾驶员并入座"))Menu_RequestAddPilot(g_npcPick);ImGui::EndDisabled();
        Hint("只会生成到空着的主驾驶座并入座。暂时不能让 NPC 自己飞去目的地。");
        if(busy)ImGui::Text("补员队列：%d / %d",filled,totalFill);ImGui::BeginDisabled(count<=0||!have||busy);if(MButton("填满空座位"))Menu_RequestFillCrew(g_npcPick);ImGui::EndDisabled();
        ImGui::BeginDisabled(count<=0);if(MButton("所有 NPC 起身",true))Menu_RequestStandAll();ImGui::SameLine();if(MButton("移除所有 NPC",true))Menu_RequestClearCrew();ImGui::EndDisabled();
        Hint("NPC 逐个生成并入座，锁住的座位不会被解锁。坐进去的 NPC 不会开船，也不会操作炮塔。");ImGui::EndPopup();}
    if(count<0){Hint("先生成一艘船，或者登船后点“使用所在飞船”。");return;}if(!count){Hint("正在等待飞船加载……");return;}
    if(MActionRow("pilot","目标飞船驾驶席","进入并通电","进入"))Menu_RequestPilot(replace);
    PrettySeatNames(seats,count,ship,pretty);MListHeader("名称","乘员");int row=0;
    for(int i=0;i<count;++i){auto&seat=seats[i];const auto name=ChineseName(Name_Seat,pretty[i]);if(!MatchesFilter(name.c_str(),filter)&&!MatchesFilter(seat.name,filter))continue;
        const char*state=seat.state==SeatState_You?"你":seat.state==SeatState_Npc?"NPC":seat.state==SeatState_Taken?"未知":seat.usable==0?"锁定":"空位";bool pick=false;
        const bool go=MActionRow(std::to_string(seat.id).c_str(),name.c_str(),state,seat.state==SeatState_You?"起身":"入座",selectedSeat==seat.id,&pick,row++,seat.name);
        if(pick)selectedSeat=seat.id;if(go){selectedSeat=seat.id;if(seat.state==SeatState_You)Menu_RequestStandUp(seat.id);else Menu_RequestSit(seat.id,replace);}
        if(selectedSeat==seat.id){
            ImGui::BeginDisabled(seat.state!=SeatState_Npc&&seat.state!=SeatState_You);if(MButton("让乘员起身",true))Menu_RequestStandUp(seat.id);ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(seat.state!=SeatState_Npc);if(MButton("移除 NPC",true))Menu_RequestKick(seat.id);ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(seat.state!=SeatState_Empty||seat.usable==0);if(MButton("添加 NPC"))Menu_RequestAddCrew(seat.id,g_npcPick);ImGui::EndDisabled();
        }
    }if(!row)Hint("没有匹配的座位。");
}
