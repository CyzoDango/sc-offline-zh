#pragma once
#include <windows.h>

struct MenuShip { char name[96]; int size; float length; float width; float height; };

int             Menu_ShipCount();
const MenuShip* Menu_Ships();
// How the spawner seats you after a ship spawns.
enum MenuSeatMode { SeatMode_None, SeatMode_Pilot, SeatMode_Named, SeatMode_PickLater };
struct MenuSpawnOptions {
    float height      = 20.0f;
    int   seatMode    = SeatMode_Pilot;
    char  seatName[48] = "";   // SeatMode_Named: words matched against the seat's entity name (e.g. "copilot", "turret left")
    bool  replaceNpc  = true;  // kick an NPC out of the seat you want instead of taking a different seat
    bool  flightReady = true;  // power the ship on once you're in a pilot seat
    int count=1;
    int direction=0; // up/down/right/left/front/back in the player's frame
    float extraGap=0;
    int relation=0; // template / enemy / civilian-neutral / friendly
    int crew=-1; // legacy template (-1), empty (0), native NPC crew manifest (1)
};
void            Menu_RequestSpawn(int index, const MenuSpawnOptions& options);

struct MenuSpawnedShip { unsigned long long id; char cls[96]; int relation,crew,state; bool item; };
// state: 0 streaming, 1 present, 2 removal requested, 3 removed, 4 not currently loaded.
int Menu_GetSpawnedShips(MenuSpawnedShip* out,int max,bool items=false);
void Menu_RequestDeleteShip(unsigned long long id);
void Menu_RequestDeleteAllShips(bool items=false);
void Menu_RequestCancelBatch();
bool Menu_SpawnBatchBusy(int* done=nullptr,int* total=nullptr);
bool Menu_FleetRecipeAvailable(int shipIndex,int relation,int crew);
float Menu_ShipSpacing(int shipIndex,float extraGap);
int Menu_SpawnItemCount();
const char* Menu_SpawnItemName(int index);
const char* Menu_SpawnItemCategory(int index);
void Menu_RequestSpawnItem(int index,int count,float distance);

// Crew & seats panel: a pinned generated ship, the ship aboard, or the latest spawn.
enum MenuSeatState { SeatState_Empty, SeatState_You, SeatState_Npc, SeatState_Taken };
struct MenuSeat { char name[64]; unsigned priority; int state; unsigned long long id; int usable = -1; };
// Independent of the Crew page's pinned target. Published after checking actual occupancy.
struct MenuPilotContext {
    bool confirmed=false;
    unsigned long long shipId=0,seatId=0;
    char shipName[96]{},seatName[64]{};
};
bool            Menu_GetCurrentPilot(MenuPilotContext& out); // cached, UI-safe, expires when stale
void            Menu_RequestCurrentFlightReady();
bool            Menu_SeatControlAvailable();
int             Menu_GetSeats(MenuSeat* out, int max, char* shipName, size_t shipNameLen); // -1 = no target ship
void            Menu_TargetShipImIn();
void            Menu_TargetSpawnedShip(unsigned long long shipId);
unsigned long long Menu_CrewTargetShip();
bool            Menu_CrewFillBusy(int* done, int* total);
void            Menu_RequestSit(unsigned long long seatId, bool replaceNpc);
void            Menu_RequestKick(unsigned long long seatId);       // removes the NPC
void            Menu_RequestStandUp(unsigned long long seatId);    // you or the NPC get out of the seat
void            Menu_RequestStandAll();
void            Menu_RequestAddCrew(unsigned long long seatId, int npcIndex);
void            Menu_RequestAddPilot(int npcIndex);
void            Menu_RequestFillCrew(int npcIndex);
void            Menu_RequestPilot(bool replaceNpc);
void            Menu_RequestClearCrew();
void            Menu_RequestFlightReady();
void            Menu_GetStatus(char* out, size_t n);
void            SetMenuStatus(const char* fmt, ...);   // also logged; shown in the menu's status strip

enum MenuGearSlot { Gear_Undersuit, Gear_Helmet, Gear_Torso, Gear_Arms, Gear_Legs, Gear_Backpack,
                    Gear_Primary, Gear_Sidearm, Gear_Ammo, Gear_Grenade, Gear_SlotCount };
int             Menu_GearCount(int slot);
const char*     Menu_GearName(int slot, int index);
void            Menu_RequestEquip(const int picks[Gear_SlotCount]);
void            Menu_RunConsole(const char* cmd);
bool            Menu_ConsoleReady();   // the game's console was found

int             Menu_NpcCount();
const char*     Menu_NpcName(int index);
void            Menu_RequestNpc(int index, int count);
void            Menu_RequestClearNpcs();

int             Menu_BuildCount();
const char*     Menu_BuildName(int index);
const char*     Menu_BuildCategory(int index);
int             Menu_BuildCategoryOf(int index);
int             Menu_BuildCategoryCount();
const char*     Menu_BuildCategoryName(int category);
void            Menu_SetBuild(int index, float reach);
float           Menu_BuildReach();
void            Menu_ToggleBuildMode();
bool            Menu_BuildModeActive();
void            Menu_BuildUndo();
void            Menu_BuildClear();
int             Menu_BuildPlacedCount();

int             Menu_OutfitCount();
const char*     Menu_OutfitName(int index);
void            Menu_RequestWearOutfit(int index);
void            Menu_SetS42VisorHud(bool on);

void Menu_Start(HWND gameWindow);
// Game thread, on every pass of the message hook: builds the menu's ImGui frame when the menu
// thread has asked for one (menu.cpp: sco.ui draws run on the game thread only).
void Menu_GameThreadFrame();
// The menu has focus and one of its text boxes is taking keys: hotkeys stay quiet.
bool Menu_Typing();
