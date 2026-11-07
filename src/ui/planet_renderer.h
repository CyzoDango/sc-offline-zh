#pragma once
#include "../third_party/imgui/imgui.h"
#include <string>
struct ID3D11Device;
struct ID3D11DeviceContext;

// Launcher-only off-screen renderer. No game handles, hooks or desktop capture.
namespace sco_v3 {
bool InitPlanets(ID3D11Device*, ID3D11DeviceContext*, const wchar_t* localTextureFolder);
void StopPlanets();
bool Planet(ImDrawList*, ImVec2 center, float radius, double latitude, double longitude, int body);
bool ValidatePlanetShaders(std::string& error);
}
