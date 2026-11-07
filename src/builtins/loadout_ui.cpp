// The loadout built-in's menu tabs: Player (noclip, god mode, infinite ammo and the gear menu) and
// Squadron 42 (outfits, the SQ42 settings, the [sq42] spawn list, SQ42 ships and the console).
// Moved from menu.cpp; registered through sco.ui by loadout_plugin.cpp and drawn by the menu shell
// on the game thread. The Player tab's noclip, god mode and infinite ammo are the optional creative
// plugin's (data/plugins/creative): the checkboxes run its commands through sco_api's invoke.
#include "tabs.h"
#include "../menu.h"
#include "../menu_ui.h"
#include "../third_party/imgui/imgui.h"
#include "../build.h"
#include "../cvars.h"
#include "../spawner.h"
#include "../zh/seat_policy.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sc_creative.h>

static const sco_api* g_api = nullptr;
static sco_plugin*    g_self = nullptr;

void RegisterPlayerTab(const sco_api* api, sco_plugin* self) {
    g_api = api;
    g_self = self;
    RegisterBuiltinTab(api, self, "loadout.player", "玩家", kTabPlayer, DrawPlayerTab);
}

// The command's reply in the status strip (which also logs it). A refusal puts the checkbox back; a
// missing command means the creative plugin isn't loaded.
static void CreativeReply(sco_result r, const char* reply, void* ctx) {
    if (r == SCO_OK) {
        SetMenuStatus("%s", reply ? reply : "");
        return;
    }
    if (ctx) *static_cast<bool*>(ctx) = !*static_cast<bool*>(ctx);
    if (reply && reply[0]) SetMenuStatus("%s", reply);
    else if (r == SCO_NOT_FOUND) SetMenuStatus("需要 creative 插件，去启动器的“插件”页开启。");
    else SetMenuStatus("creative 插件没有执行（返回 %d）。", static_cast<int>(r));
}

void InvokeCreativeToggle(const char* command, bool on, bool* flag) {
    if (!g_api || !g_self) { if (flag) *flag = !on; return; }
    sco_arg arg = {};
    arg.type = SCO_ARG_BOOL;
    arg.v.i = on ? 1 : 0;
    g_api->invoke(g_self, command, &arg, 1, CreativeReply, flag);
}

void InvokeCreativeSpeed(float metresPerSecond) {
    if (!g_api || !g_self) return;
    sco_arg arg = {};
    arg.type = SCO_ARG_FLOAT;
    arg.v.f = metresPerSecond;
    g_api->invoke(g_self, "creative.noclip_speed", &arg, 1, CreativeReply, nullptr);
}

static volatile LONG g_currentAmmoRequest=0,g_currentAmmoOn=0,g_currentAmmoKnown=0;
static unsigned long long g_currentAmmoShip=0,g_currentAmmoSeat=0;
static bool g_currentAmmoOwned=false;
static const sc_creative_v1* CurrentAmmoService(){
    if(!g_api || !g_self || g_api->size<=offsetof(sco_api,query_service))return nullptr;
    const sc_creative_v1* service=nullptr;
    return g_api->query_service(SC_CREATIVE_SERVICE_NAME,SC_CREATIVE_SERVICE_VERSION,
        reinterpret_cast<const void**>(&service))==SCO_OK?service:nullptr;
}
bool Menu_CurrentShipAmmoKnown(){return g_currentAmmoKnown!=0;}
bool Menu_CurrentShipAmmoOn(){return g_currentAmmoOn!=0;}
void Menu_RequestCurrentShipAmmo(bool on){InterlockedExchange(&g_currentAmmoRequest,on?1:2);}
void RestoreCurrentShipAmmo(){
    if(!g_currentAmmoOwned)return;
    const auto* service=CurrentAmmoService();uint32_t flags=0;
    if(service && service->set_infinite_ship_ammo(g_self,0)==SCO_OK && service->state(g_self,&flags)==SCO_OK &&
       !(flags&SC_CREATIVE_INFINITE_SHIP_AMMO)){
        g_currentAmmoOwned=false;g_currentAmmoShip=g_currentAmmoSeat=0;InterlockedExchange(&g_currentAmmoOn,0);
        Log("[pilot] restored owned ship-ammo override");
    }
}
void ProcessCurrentShipAmmo(const MenuPilotContext& context){
    const LONG request=InterlockedExchange(&g_currentAmmoRequest,0);MenuPilotContext pilot=context;
    if(request==1)CurrentPilotNow(pilot,true); // recheck on the game thread, not just the UI snapshot
    const auto* service=CurrentAmmoService();uint32_t flags=0;
    const bool known=service && service->state(g_self,&flags)==SCO_OK;
    InterlockedExchange(&g_currentAmmoKnown,known?1:0);
    if(g_currentAmmoOwned && (!sco_zh::SamePilotLease(pilot.confirmed,pilot.shipId,pilot.seatId,g_currentAmmoShip,g_currentAmmoSeat) || request==2))
        RestoreCurrentShipAmmo();
    if(request!=1)return;
    if(!pilot.confirmed){SetMenuStatus("请先坐进主驾驶座，飞船弹药未改动。");return;}
    if(!known){SetMenuStatus("当前版本的飞船弹药服务不可用。");return;}
    if(g_currentAmmoOwned){SetMenuStatus("上次的飞船弹药设置还没恢复，稍等一下再试。");return;}
    if(flags&SC_CREATIVE_INFINITE_SHIP_AMMO){SetMenuStatus("飞船弹药是在别处开启的，这里接管不了，先去原来的地方关掉。");return;}
    // Take ownership only from the disabled state, so leaving the seat never turns
    // off an override that was already active through another entry point.
    g_currentAmmoOwned=true;g_currentAmmoShip=pilot.shipId;g_currentAmmoSeat=pilot.seatId;
    const bool applied=service->set_infinite_ship_ammo(g_self,1)==SCO_OK && service->state(g_self,&flags)==SCO_OK &&
                       (flags&SC_CREATIVE_INFINITE_SHIP_AMMO);
    if(!applied)RestoreCurrentShipAmmo();
    else InterlockedExchange(&g_currentAmmoOn,1);
    SetMenuStatus(applied?"已为当前驾驶的飞船开启无限弹药，离座或换船会自动关闭。":"飞船无限弹药没开成，看一下日志。");
}

bool VerifyCreativeShipAmmo(){
    if(!g_api || !g_self || g_api->size<=offsetof(sco_api,query_service))return false;
    const sc_creative_v1* service=nullptr;
    if(g_api->query_service(SC_CREATIVE_SERVICE_NAME,SC_CREATIVE_SERVICE_VERSION,reinterpret_cast<const void**>(&service))!=SCO_OK || !service)return false;
    uint32_t before=0,on=0,after=0;
    if(service->state(g_self,&before)!=SCO_OK)return false;
    if(before&SC_CREATIVE_INFINITE_SHIP_AMMO)return false; // don't take ownership of another active toggle
    const sco_result applied=service->set_infinite_ship_ammo(g_self,1);
    const bool readOn=service->state(g_self,&on)==SCO_OK;
    const sco_result restored=service->set_infinite_ship_ammo(g_self,0);
    const bool readAfter=service->state(g_self,&after)==SCO_OK;
    const bool ok=applied==SCO_OK && readOn && (on&SC_CREATIVE_INFINITE_SHIP_AMMO) && restored==SCO_OK && readAfter && before==after;
    Log("[zh-verify] creative.ship_ammo %s: flags %u -> %u -> %u",ok?"PASS":"FAIL",before,on,after);
    return ok;
}


void DrawPlayerTab(void*,void*){
    static bool noclip=false,god=false,ammo=false;static float speed=30;
    static int gear[Gear_SlotCount]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},slot=-1;static char filter[96]="";
    SearchBox("##gearFilter","搜索装备中文名或英文名",filter,sizeof(filter));
    SectionHeading("移动");if(MSetting("穿墙","creative · F7 保存位置，F8 返回",&noclip)){InvokeCreativeSpeed(speed);InvokeCreativeToggle("creative.noclip",noclip,&noclip);}
    if(MSliderFloat("移动速度##noclipSpeed",&speed,1,500,"%.0f",ImGuiSliderFlags_Logarithmic))InvokeCreativeSpeed(speed);
    SectionHeading("保护");if(MSetting("无敌模式","需要在启动器启用 creative 插件",&god))InvokeCreativeToggle("creative.god",god,&god);
    if(MSetting("无限弹药","玩家携带武器的弹药",&ammo))InvokeCreativeToggle("creative.ammo",ammo,&ammo);
    SectionHeading("装备");const char*labels[]={"基底服","头盔","胸甲","臂甲","腿甲","背包","主武器","副武器","弹药","手雷"};
    if(Menu_GearCount(0)<0){Hint("正在加载装备，需要先进入游戏场景。");return;}MListHeader();
    for(int i=0;i<Gear_SlotCount;++i){if(gear[i]>=Menu_GearCount(i))gear[i]=-1;const std::string name=std::string(labels[i])+" · "+(gear[i]>=0?ChineseName(Name_Item,Menu_GearName(i,gear[i])):i==0?"默认":i==8?"匹配武器":"无");
        if(MActionRow(std::to_string(i).c_str(),name.c_str(),"装备槽","选择",false,nullptr,i,name.c_str())){slot=i;ImGui::OpenPopup("gear-picker");}}
    ImGui::SetNextWindowSizeConstraints({Ui(400),0},{Ui(560),Ui(380)});
    if(MPopup("gear-picker")){if(slot>=0){SectionHeading(labels[slot]);if(V3Selectable(slot==0?"默认":slot==8?"匹配武器":"无",gear[slot]<0,0)){gear[slot]=-1;ImGui::CloseCurrentPopup();}
        int row=0;for(int i=0;i<Menu_GearCount(slot);++i){const char*name=Menu_GearName(slot,i);if(!MatchesName(Name_Item,name,filter))continue;ImGui::PushID(i);if(V3Selectable(ChineseName(Name_Item,name).c_str(),gear[slot]==i,row++)){gear[slot]=i;ImGui::CloseCurrentPopup();}ImGui::PopID();}}
        ImGui::EndPopup();}
    if(MActionRow("equip","当前装备方案","整套穿戴","应用")){Menu_RequestEquip(gear);MenuClose();}
}
void DrawSq42Tab(void*,void*){
    static bool spoilerOk=false;static char filter[96]="";static int view=0;
    if(!spoilerOk){SectionHeading("剧透提示");Hint("此页面包含《42 中队》的模型、服装和设置，可能剧透。");if(MActionRow("accept","继续查看 42 中队","包含剧透","确定"))spoilerOk=true;if(MActionRow("back","返回普通菜单","玩家","返回"))MenuSelectFirstTab();return;}
    SearchBox("##sq42Search","搜索 42 中队服装、对象、飞船或设置",filter,sizeof(filter));
    const char*views[]={"服装","对象","飞船","设置","控制台"};for(int i=0;i<5;++i){if(i)ImGui::SameLine();if(MChip(views[i],view==i))view=i;}
    if(view==0){static bool visor=false;if(MSetting("面罩 HUD","下次更换装备或服装生效",&visor))Menu_SetS42VisorHud(visor);
        const int n=Menu_OutfitCount();if(n<=0){Hint(n<0?"正在加载服装……":"未找到服装，请检查 outfits.txt。");return;}MListHeader();int row=0;
        for(int i=0;i<n;++i){const char*name=Menu_OutfitName(i);if(!MatchesName(Name_Outfit,name,filter))continue;
            if(MActionRow(std::to_string(i).c_str(),ChineseName(Name_Outfit,name).c_str(),"42 中队服装","应用",false,nullptr,row++,name)){Menu_RequestWearOutfit(i);MenuClose();}}
        if(!row)Hint("没有匹配的服装。");
    }else if(view==1){static bool front=true;static float ahead=8;if(MButton("放置设置"))ImGui::OpenPopup("sq42-options");
        if(MPopup("sq42-options")){V3Toggle("在面前生成",&front);ImGui::BeginDisabled(!front);ImGui::SetNextItemWidth(Ui(360));MSliderFloat("前方距离##ahead",&ahead,1,50,"%.0f 米");ImGui::EndDisabled();ImGui::EndPopup();}
        const int n=Menu_BuildCount();if(n<0){Hint("正在加载对象……");return;}MListHeader();int row=0;
        for(int i=0;i<n;++i){if(_stricmp(Menu_BuildCategory(i),"sq42"))continue;const char*name=Menu_BuildName(i);if(!MatchesName(Name_Build,name,filter))continue;
            if(MActionRow(std::to_string(i).c_str(),ChineseName(Name_Build,name).c_str(),"42 中队对象","生成",false,nullptr,row++,name)){Menu_RequestPlace(i,front,ahead);MenuClose();}}
        Hint("这里生成的对象可以在“建造”页用“撤销”或“清空”删掉。");
    }else if(view==2){
    struct Entry { const char* label; const char* cls; bool enemyWing; float height; bool sit; };
    static const struct { const char* label; const char* cls; } kSq42Ships[] = {
        { "伊德里斯-P（斯坦顿号同型舰）", "AEGS_Idris_P" },
        { "短剑（42 中队战斗机）",        "AEGS_Gladius" },
        { "报复者（带 42 中队 HUD）",   "AEGS_Retaliator" },
        { "星际远航者（第 5、7、9 章）",        "MISC_Starfarer" },
        { "复仇者 追猎（42 中队残骸）",   "AEGS_Avenger_Stalker" },
        { "大黄蜂（卡尔·梅森的飞船）",     "ANVL_Hornet_F7C" },
        { "梵杜尔 刀锋（AI）",            "VNCL_Blade_PU_AI_VAN" },
        { "梵杜尔 镰刀（AI）",           "VNCL_Scythe_PU_AI_VAN" },
        { "梵杜尔 阔剑（AI）",           "VNCL_Glaive_PU_AI_VAN" },
        { "梵杜尔 毒刺（AI）",          "VNCL_Stinger_PU_AI_VAN" },
    };
    Entry list[16];
    static_assert(sizeof(kSq42Ships) / sizeof(kSq42Ships[0]) + 2 <= sizeof(list) / sizeof(list[0]),
                  "SQ42 ship table outgrew Entry list[]");
    int   n = 0;
    for (size_t i = 0; i < sizeof(kSq42Ships) / sizeof(kSq42Ships[0]); ++i)
        list[n++] = { kSq42Ships[i].label, kSq42Ships[i].cls, false, 0.0f, true };
    for (int i = 0; i < n; ++i)
        if (strncmp(list[i].cls, "VNCL_", 5) == 0) { list[i].height = 300.0f; list[i].sit = false; }

    // Not the original's dormant "[battle]" mode (two Bengals fighting each other): these
    // spawn one UEE Bengal, the second with a Vanduul wing when an enemy side was found.
    static char bengalWing[64];
    const bool enemySide = Menu_EnemySideAvailable();
    strcpy_s(bengalWing, enemySide ? "孟加拉与梵杜尔编队" : "孟加拉与编队（UEE，未找到敌方阵营）");
    list[n++] = { "孟加拉（UEE）", "RSI_Bengal_PU_AI_UEE", false,      1500.0f, false };
    list[n++] = { bengalWing,     "RSI_Bengal_PU_AI_UEE", enemySide, 1500.0f, false };


    static float height=30;static bool sit=true;
    auto known = [](const char* cls) {
        const int count = Menu_ShipCount();
        if (count < 0) return true;   // ship list still loading: don't block
        const MenuShip* ships = Menu_Ships();
        for (int i = 0; i < count; ++i)
            if (_stricmp(ships[i].name, cls) == 0) return true;
        return false;
    };


        if(MButton("登船设置"))ImGui::OpenPopup("sq42-ship-options");if(MPopup("sq42-ship-options")){ImGui::SetNextItemWidth(Ui(400));MSliderFloat("生成高度##height",&height,0,500,"%.0f 米");V3Toggle("把我放进驾驶座",&sit);ImGui::EndPopup();}
        MListHeader();int row=0;for(int i=0;i<n;++i){if(!MatchesFilter(list[i].label,filter))continue;const bool have=known(list[i].cls);ImGui::BeginDisabled(!have);
            const bool fire=MActionRow(std::to_string(i).c_str(),list[i].label,list[i].sit?"己方飞船":"AI／编队","生成",false,nullptr,row++,list[i].cls);ImGui::EndDisabled();
            if(fire){Menu_RequestSpawnClass(list[i].cls,list[i].sit?height:list[i].height,list[i].sit&&sit,list[i].sit&&sit,list[i].enemyWing);MenuClose();}}
    }else if(view==3){SectionHeading("42 中队设置");for(int i=0;i<Menu_S42SettingCount();++i){if(!MatchesFilter(Menu_S42SettingLabel(i),filter))continue;ImGui::PushID(i);bool on=Menu_S42SettingOn(i);const bool known=Menu_S42SettingKnown(i);ImGui::BeginDisabled(!known);
        if(MSetting(Menu_S42SettingLabel(i),known?Menu_S42SettingTip(i):"正在读取游戏设置",&on))Menu_RequestS42Setting(i,on);ImGui::EndDisabled();ImGui::PopID();}
    }else{SectionHeading("游戏控制台");static char command[256]="";const bool ready=Menu_ConsoleReady();ImGui::BeginDisabled(!ready);ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Ui(70));
        bool run=MTextField("##console","输入游戏控制台命令",command,sizeof(command),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();run|=MButton("执行");ImGui::EndDisabled();
        if(run&&command[0]){Menu_RunConsole(command);command[0]=0;}Hint(ready?"命令在游戏控制台执行，结果写入游戏日志。":"还没找到游戏控制台。");}
}
