#include "cvars.h"
#include "menu.h"
#include "menu_ui.h"
#include "world_caps.h"
#include "zh/session_setting.h"
#include "zh/seat_policy.h"
#include "spawner.h"

// Each cvar's storage is a sco-core row (cvars.*, sco/game/world.h), in capability cvars.qdrive_kept_on.
static const struct { const char* cvar; int32_t value; const char* also; const char* row; } kKeptOn[] = {
    { "v_qdrive2.quantumTravelAllowed", 1, "p_enable_physical_quantum_travel 1", "cvars.quantum_travel_allowed" },
    { "v_qdrive2.quantumBoostAllowed", 1, nullptr, "cvars.quantum_boost_allowed" },
    { "v_qdrive2.setting_ignoreBlockedBoost", 1, nullptr, "cvars.ignore_blocked_boost" },
    { "v_qdrive2.setting_ignoreBlockedTravel", 1, nullptr, "cvars.ignore_blocked_travel" },
    { "v_qdrive.logging", 1, nullptr, "cvars.qdrive_logging" },
};
constexpr int kKeptOnCount = sizeof(kKeptOn) / sizeof(kKeptOn[0]);
static int32_t* g_keptOn[kKeptOnCount];

static const struct { const char* cvar; float value; const char* row; } kKeptOnFloat[] = {
    { "v_qdrive2.setting_targetLockAngularSpeedThresholdPlayer", 45.0f, "cvars.target_lock_angular" },
    { "v_qdrive2.setting_targetLockLinearSpeedThresholdPlayer", 1000.0f, "cvars.target_lock_linear" },
};
constexpr int kKeptOnFloatCount = sizeof(kKeptOnFloat) / sizeof(kKeptOnFloat[0]);
static float* g_keptOnFloat[kKeptOnFloatCount];

static uintptr_t* g_console = nullptr;
static int32_t* g_offlineHeat = nullptr;

static void FindConsole() {
    if (WorldCapability("cvars.console")) { g_console = reinterpret_cast<uintptr_t*>(sco::Sig("cvars.console")); return; }
    Log("[cvars] console not found; console commands are disabled (see the [core] lines in mod.log)");
}

static SRWLOCK       g_lock = SRWLOCK_INIT;
static char          g_command[256];
static volatile LONG g_commandPending = 0;

bool Menu_ConsoleReady() { return g_console && *g_console; }

void Menu_RunConsole(const char* cmd) {
    AcquireSRWLockExclusive(&g_lock);
    strncpy_s(g_command, cmd, _TRUNCATE);
    ReleaseSRWLockExclusive(&g_lock);
    InterlockedExchange(&g_commandPending, 1);
}

static bool Execute(const char* cmd) {
    if (!g_console || !*g_console) return false;
    __try {
        VCall<void>(*g_console, 0x130, cmd, false, false, static_cast<void*>(nullptr));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[cvars] fault while running '%s'", cmd);
        return false;
    }
}

bool SetCVarNow(const char* name, float value) {
    if (!g_console || !*g_console) return false;
    __try {
        const uintptr_t cvar = VCall<uintptr_t>(*g_console, 0xC0, name);
        if (!cvar) return false;
        VCall<void>(cvar, 0x38, value);
        return VCall<float>(cvar, 0x20) == value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GetCVarNow(const char* name, float& value) {
    if (!g_console || !*g_console || !name || !*name) return false;
    __try {
        const uintptr_t cvar = VCall<uintptr_t>(*g_console, 0xC0, name);
        if (!cvar) return false;
        value = VCall<float>(cvar, 0x20);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The ordinary setter rejects VF_NET_SYNCED rules without a server, even in an
// offline game. Change only this verified int-backed local rule, like the
// existing kept-on quantum cvars. Its flags and every other cvar stay intact.
static bool SetOfflineHeatNow(float value){
    if(!g_offlineHeat || (value!=0.0f && value!=1.0f))return false;
    wchar_t boot[32]={};
    if(!GetEnvironmentVariableW(L"SC_OFFLINE_BOOT_MAP",boot,32))return false;
    MEMORY_BASIC_INFORMATION page={};
    if(!VirtualQuery(g_offlineHeat,&page,sizeof(page)) || page.State!=MEM_COMMIT ||
       (page.Protect&PAGE_GUARD) || !(page.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return false;
    float current=0;
    if(!GetCVarNow("i_heat.enableOverheat",current))return false;
    __try{
        if((*g_offlineHeat!=0 && *g_offlineHeat!=1) || current!=static_cast<float>(*g_offlineHeat))return false;
        *g_offlineHeat=static_cast<int32_t>(value);
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    float verified=0;
    return GetCVarNow("i_heat.enableOverheat",verified) && verified==value;
}

// Squadron 42 tab settings. The shipped mod carries these four cvars XORed with a
// per-blob key (Murmur-style fmix32 over the byte offset); these are the names
// decrypted out of dinput8.dll's label table.
static const struct { const char* label; const char* tip; const char* cvar; } kS42Settings[] = {
    { "42 中队自动锁定",
      "开启 42 中队的自动锁定功能，游戏模式可能覆盖此设置。",
      "i_target_selector.targeting2_enabled" },
    { "面罩小地图",
      "面罩 HUD 上的小地图。",
      "pl_lensdisplay.minimap_enabled" },
    { "面罩装饰框架",
      "面罩 HUD 周围的装饰框架。",
      "pl_lensdisplay.greebles_enabled" },
    { "42 中队菜单",
      "将暂停菜单和加载画面切换为 42 中队版本。部分数据缺失，如遇异常，请先关闭再旅行或退出。",
      "g_squadron_frontend" },
};
constexpr int kS42SettingCount = sizeof(kS42Settings) / sizeof(kS42Settings[0]);

static volatile LONG g_s42On[kS42SettingCount] = { -1, -1, -1, -1 };
static volatile LONG g_s42PendingMask = 0;   // bit i = a toggle is waiting
static volatile LONG g_s42PendingVal  = 0;   // bit i = the value it wants

int         Menu_S42SettingCount() { return kS42SettingCount; }
const char* Menu_S42SettingLabel(int i) { return i >= 0 && i < kS42SettingCount ? kS42Settings[i].label : ""; }
const char* Menu_S42SettingTip(int i)   { return i >= 0 && i < kS42SettingCount ? kS42Settings[i].tip   : ""; }
bool        Menu_S42SettingOn(int i)    { return i >= 0 && i < kS42SettingCount && g_s42On[i] == 1; }
bool        Menu_S42SettingKnown(int i) { return i >= 0 && i < kS42SettingCount && g_s42On[i] >= 0; }

// One pending slot per toggle rather than one overall, so two toggles inside the
// same game tick both land. Value first, mask second: a reader that sees the bit
// must already see the value.
void Menu_RequestS42Setting(int i, bool on) {
    if (i < 0 || i >= kS42SettingCount) return;
    if (on) InterlockedOr(&g_s42PendingVal, 1L << i);
    else    InterlockedAnd(&g_s42PendingVal, ~(1L << i));
    InterlockedOr(&g_s42PendingMask, 1L << i);
}

static void ApplyS42Setting(int i, bool on) {
    const int want = on ? 1 : 0;
    if (!SetCVarNow(kS42Settings[i].cvar, static_cast<float>(want))) {
        char cmd[96];
        snprintf(cmd, sizeof(cmd), "%s %d", kS42Settings[i].cvar, want);
        if (!Execute(cmd)) { Log("[sq42] console unavailable: %s", cmd); return; }
    }
    Log("[sq42] %s = %d", kS42Settings[i].cvar, want);
}

// Game thread only: reads the four cvars back so the menu can show them. Capped at
// 5 Hz — four name lookups every tick buys nothing the menu can see.
static void RefreshS42Settings() {
    static DWORD last = 0;
    const DWORD now = GetTickCount();
    if (now - last < 200) return;
    last = now;
    for (int i = 0; i < kS42SettingCount; ++i) {
        float v = 0.0f;
        const bool ok = GetCVarNow(kS42Settings[i].cvar, v);
        InterlockedExchange(&g_s42On[i], ok && v != 0.0f ? 1 : 0);
    }
}

// Verified by name through the game's console registry, never by guessed memory
// offsets. Scene-wide developer switches, not per-ship power/heat component edits.
static const struct { const char* name; const char* label; float wanted; } kShipSettings[Ship_SettingCount] = {
    { "i_heat.enableOverheat", "不过热", 0.0f },
    { "g_AIComponents.seatOperator.useInfiniteAmmo", "AI 炮手无限弹药", 1.0f },
    { "item_resource.force_cheat", "无限电力", 1.0f },
};
static sco_zh::SessionSetting g_shipSettings[Ship_SettingCount];
static volatile LONG g_shipKnown[Ship_SettingCount]={},g_shipOn[Ship_SettingCount]={};
static volatile LONG g_shipMask=0,g_shipVals=0;
static unsigned long long g_shipOverrideShip=0,g_shipOverrideSeat=0;
bool Menu_ShipSettingKnown(int i){return i>=0 && i<Ship_SettingCount && g_shipKnown[i]!=0;}
bool Menu_ShipSettingOn(int i){return i>=0 && i<Ship_SettingCount && g_shipOn[i]!=0;}
void Menu_RequestShipSetting(int i,bool on){
    if(i<0 || i>=Ship_SettingCount)return;
    if(on)InterlockedOr(&g_shipVals,1L<<i);else InterlockedAnd(&g_shipVals,~(1L<<i));
    InterlockedOr(&g_shipMask,1L<<i);
}
static bool ChangeShipSetting(int i,bool on){
    const auto read=[&](float& value){return GetCVarNow(kShipSettings[i].name,value);};
    const auto write=[&](float value){return i==Ship_NoOverheat?SetOfflineHeatNow(value):SetCVarNow(kShipSettings[i].name,value);};
    const bool ok=g_shipSettings[i].Set(on,kShipSettings[i].wanted,read,write);
    if(!ok && on)g_shipSettings[i].Set(false,kShipSettings[i].wanted,read,write);
    InterlockedExchange(&g_shipOn[i],g_shipSettings[i].Saved()?1:0);
    float actual=0;const bool readable=read(actual);
    Log("[zh-ship] %s requested=%s result=%s current=%g pending_restore=%d",kShipSettings[i].name,on?"on":"off",ok?"ok":"failed",actual,g_shipSettings[i].Saved()?1:0);
    if(!ok)SetMenuStatus("%s未能应用或恢复，请查看日志。",kShipSettings[i].label);
    else SetMenuStatus("%s：%s（%s=%g）",kShipSettings[i].label,on?"已应用设置":"已恢复原值",kShipSettings[i].name,readable?actual:0);
    return ok;
}
void RestoreShipSettings(){
    for(int i=0;i<Ship_SettingCount;++i)if(g_shipSettings[i].Saved())ChangeShipSetting(i,false);
    RestoreCurrentShipAmmo();
}
static void ProcessShipSettings(){
    const LONG mask=InterlockedExchange(&g_shipMask,0),values=g_shipVals;
    MenuPilotContext pilot;CurrentPilotNow(pilot,(mask&values)!=0);
    ProcessCurrentShipAmmo(pilot);
    const auto saved=[](){for(const auto& setting:g_shipSettings)if(setting.Saved())return true;return false;};
    const bool lease=sco_zh::SamePilotLease(pilot.confirmed,pilot.shipId,pilot.seatId,g_shipOverrideShip,g_shipOverrideSeat);
    if(saved() && !lease){
        static DWORD lastRestore=0;const DWORD now=GetTickCount();
        if(now-lastRestore>=250){lastRestore=now;
            for(int i=0;i<Ship_SettingCount;++i)if(g_shipSettings[i].Saved())ChangeShipSetting(i,false);
        }
    }
    for(int i=0;i<Ship_SettingCount;++i)if(mask&(1L<<i)){
        const bool on=(values&(1L<<i))!=0;
        if(on && (!pilot.confirmed || (saved() && !sco_zh::SamePilotLease(pilot.confirmed,pilot.shipId,pilot.seatId,g_shipOverrideShip,g_shipOverrideSeat)))){SetMenuStatus("主驾驶身份未确认或原设置仍待恢复，未开启飞船选项。");continue;}
        if(on){g_shipOverrideShip=pilot.shipId;g_shipOverrideSeat=pilot.seatId;}
        ChangeShipSetting(i,on);
    }
    static DWORD last=0;const DWORD now=GetTickCount();if(now-last<1000)return;last=now;
    for(int i=0;i<Ship_SettingCount;++i){
        float current=0;const bool known=GetCVarNow(kShipSettings[i].name,current) && (i!=Ship_NoOverheat || g_offlineHeat);
        if(InterlockedExchange(&g_shipKnown[i],known?1:0)==0 && known)
            Log("[zh-ship] registry verified %s=%g",kShipSettings[i].name,current);
        // Reapply only when this override owns a snapshot and the game changed it.
        if(known && pilot.confirmed && sco_zh::SamePilotLease(true,pilot.shipId,pilot.seatId,g_shipOverrideShip,g_shipOverrideSeat) &&
           g_shipSettings[i].Enabled() && current!=kShipSettings[i].wanted)ChangeShipSetting(i,true);
    }
    // Explicit developer check, off by default. Exercises the real registry,
    // including readback of both application and exact restoration, once.
    static bool verified=false;
    wchar_t check[4]={};
    if(!verified && GetEnvironmentVariableW(L"SCO_ZH_VERIFY_SWITCHES",check,4)==1 && check[0]==L'1'){
        bool ready=pilot.confirmed;for(int i=0;i<Ship_SettingCount;++i)ready=ready && g_shipKnown[i] && !g_shipSettings[i].Saved();
        if(ready){
            verified=true;int passed=0;
            for(int i=0;i<Ship_SettingCount;++i){
                float before=0,applied=0,restored=0;
                const bool captured=GetCVarNow(kShipSettings[i].name,before);
                const bool appliedOk=captured && ChangeShipSetting(i,true) && GetCVarNow(kShipSettings[i].name,applied) && applied==kShipSettings[i].wanted;
                const bool restoredOk=ChangeShipSetting(i,false) && GetCVarNow(kShipSettings[i].name,restored) && restored==before;
                const bool ok=captured && appliedOk && restoredOk;
                if(ok)++passed;
                Log("[zh-verify] %s %s: %g -> %g -> %g",kShipSettings[i].name,ok?"PASS":"FAIL",before,applied,restored);
            }
            int pending=0;for(int i=0;i<Ship_SettingCount;++i)if(g_shipSettings[i].Saved())++pending;
            Log("[zh-verify] actual game registry roundtrips: %d/%d passed; pending_snapshots=%d",passed,Ship_SettingCount,pending);
            const bool ammoOk=VerifyCreativeShipAmmo();
            Log("[zh-verify] four requested controls: %d/4 roundtrips passed",passed+(ammoOk?1:0));
        }
    }
}

void ResolveCVarsApi(const Section&, const Section&) {
    if(WorldCapability("cvars.offline_heat"))g_offlineHeat=reinterpret_cast<int32_t*>(sco::Sig("cvars.offline_overheat_storage"));
    if (WorldCapability("cvars.qdrive_kept_on")) {
        for (int i = 0; i < kKeptOnCount; ++i) g_keptOn[i] = reinterpret_cast<int32_t*>(sco::Sig(kKeptOn[i].row));
        for (int i = 0; i < kKeptOnFloatCount; ++i) g_keptOnFloat[i] = reinterpret_cast<float*>(sco::Sig(kKeptOnFloat[i].row));
    } else {
        Log("[+] quantum cvars not found; they stay as the game mode sets them (see the [core] lines in mod.log)");
    }
    FindConsole();
}

static void KeepSettingsOn() {
    for (int i = 0; i < kKeptOnCount; ++i) {
        int32_t* const s = g_keptOn[i];
        if (!s) continue;
        __try {
            if (*s == kKeptOn[i].value) continue;
            char cmd[96];
            snprintf(cmd, sizeof(cmd), "%s %d", kKeptOn[i].cvar, kKeptOn[i].value);
            Execute(cmd);
            if (*s != kKeptOn[i].value) *s = kKeptOn[i].value;
            if (kKeptOn[i].also) Execute(kKeptOn[i].also);
            Log("[+] %s = %d%s%s", kKeptOn[i].cvar, *s, kKeptOn[i].also ? ", " : "", kKeptOn[i].also ? kKeptOn[i].also : "");
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    for (int i = 0; i < kKeptOnFloatCount; ++i) {
        float* const s = g_keptOnFloat[i];
        if (!s) continue;
        __try {
            if (*s == kKeptOnFloat[i].value) continue;
            char cmd[96];
            snprintf(cmd, sizeof(cmd), "%s %g", kKeptOnFloat[i].cvar, kKeptOnFloat[i].value);
            Execute(cmd);
            if (*s != kKeptOnFloat[i].value) *s = kKeptOnFloat[i].value;
            Log("[+] %s = %g", kKeptOnFloat[i].cvar, *s);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

void ProcessCVars() {
    KeepSettingsOn();
    ProcessShipSettings();
    {
        const LONG mask = InterlockedExchange(&g_s42PendingMask, 0);
        const LONG vals = g_s42PendingVal;
        for (int i = 0; i < kS42SettingCount; ++i)
            if (mask & (1L << i)) ApplyS42Setting(i, (vals >> i) & 1);
        RefreshS42Settings();
    }
    if (InterlockedExchange(&g_commandPending, 0)) {
        char cmd[256];
        AcquireSRWLockExclusive(&g_lock);
        strcpy_s(cmd, g_command);
        ReleaseSRWLockExclusive(&g_lock);
        if (Execute(cmd)) Log("[cvars] console: %s", cmd);
        else              Log("[cvars] console unavailable: %s", cmd);
    }
}
