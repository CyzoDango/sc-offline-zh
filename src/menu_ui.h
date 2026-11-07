#pragma once
// The menu shell's shared look and widgets (menu.cpp), for the tabs the built-ins draw through
// sco.ui (src/builtins/*_ui.cpp). Call them only from a draw callback: on the game thread, inside
// the menu's ImGui frame.
#include "third_party/imgui/imgui.h"
#include <cstddef>
#include <string>

extern const ImVec4 kMuted;      // hints
extern const ImVec4 kLeaf;       // accent
extern const ImVec4 kLeafDeep;

void  SectionHeading(const char* title);
void  Hint(const char* text);
float Ui(float logicalPixels);
bool  PrimaryButton(const char* label, float height = 26.0f);
bool MButton(const char* label, bool secondary = false);
bool MBeginCombo(const char* id, const char* preview, ImGuiComboFlags flags = 0);
bool MPopup(const char* id);
bool MCombo(const char* id, int* value, const char* const* items, int count);
bool MSliderFloat(const char* label, float* value, float minimum, float maximum, const char* format = "%.0f", ImGuiSliderFlags flags = 0);
bool MSliderInt(const char* label, int* value, int minimum, int maximum, const char* format = "%d");
bool MTextField(const char* id, const char* hint, char* buffer, size_t size, ImGuiInputTextFlags flags = 0);
void MListHeader(const char* name = "名称", const char* category = "类别");
bool MActionRow(const char* id, const char* name, const char* category, const char* action,
                bool selected = false, bool* picked = nullptr, int row = 0, const char* tip = nullptr);
bool MSetting(const char* label, const char* description, bool* value);
bool MChip(const char* label, bool selected);
bool  V3Toggle(const char* label, bool* value);
bool  V3Selectable(const char* label, bool selected, int visibleRow, ImGuiSelectableFlags flags = 0);
float Columns(int n);             // the width of one of n equal columns in the row
bool  SearchBox(const char* id, const char* hint, char* buf, size_t n);
bool  MatchesFilter(const char* name, const char* filter);   // every word of filter, any case
const std::string& ChineseShipLabel(const char* className);
bool MatchesShipFilter(const char* className, const char* filter);
int ShipModelCount();
const char* ShipModelName(int model);
int ShipModelVariantCount(int model);
int ShipModelVariant(int model,int variant); // original Menu_Ships index
std::string ShipVariantName(const char* cls);
bool ShipVersionMatches(const char* cls,int filter);
enum LocalNameKind { Name_Item, Name_Place, Name_System, Name_Npc, Name_Build, Name_Outfit, Name_Seat, Name_Category };
const std::string& ChineseName(LocalNameKind kind, const char* original);
bool MatchesName(LocalNameKind kind, const char* original, const char* filter);
std::string ChinesePlace(const char* name, const char* entity);
enum ShipSetting { Ship_NoOverheat, Ship_AIGunnerAmmo, Ship_InfinitePower, Ship_SettingCount };
bool Menu_ShipSettingKnown(int index);
bool Menu_ShipSettingOn(int index);
void Menu_RequestShipSetting(int index, bool on);
bool Menu_CurrentShipAmmoKnown();
bool Menu_CurrentShipAmmoOn();
void Menu_RequestCurrentShipAmmo(bool on);
struct MenuPilotContext;
void ProcessCurrentShipAmmo(const MenuPilotContext& pilot); // game thread only
void RestoreCurrentShipAmmo();
bool VerifyCreativeShipAmmo(); // opt-in developer check, on the game thread only
// "PlayerDeco_Crate_{1234...}.socpak" -> "Crate (1234)": a game class or file name for a list.
void  PrettyBuildName(char* out, size_t n, const char* name);

// Closes the menu after this frame (a spawn or equip that should show the game).
void  MenuClose();
// Selects the first tab in the next frame (the Squadron 42 spoiler gate's Back).
void  MenuSelectFirstTab();
