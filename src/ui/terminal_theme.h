#pragma once
#include "../third_party/imgui/imgui.h"
#include <cmath>

// Original vector artwork; shared by the launcher and the in-game terminal.
namespace sco_terminal {
inline ImVec4 Rgb(unsigned v, float a = 1) {
    return {float(v >> 16 & 255)/255, float(v >> 8 & 255)/255, float(v & 255)/255, a};
}
inline ImU32 Ink(unsigned v, unsigned a = 255) { return IM_COL32(v >> 16 & 255, v >> 8 & 255, v & 255, a); }
inline void Frame(ImDrawList* d, ImVec2 a, ImVec2 b, ImU32 fill, ImU32 line, float radius = 20) {
    const float r=fminf(radius,fminf(b.x-a.x,b.y-a.y)*.5f);
    ImVec2 points[100];int count=0;
    const ImVec2 centers[]={{b.x-r,a.y+r},{b.x-r,b.y-r},{a.x+r,b.y-r},{a.x+r,a.y+r}};
    for(int corner=0;corner<4;++corner)for(int i=0;i<=24;++i){
        const float t=(-1.5707963f+corner*1.5707963f)+i*1.5707963f/24;
        const float x=cosf(t),y=sinf(t);
        points[count++]={centers[corner].x+copysignf(powf(fabsf(x),.4f),x)*r,centers[corner].y+copysignf(powf(fabsf(y),.4f),y)*r};
    }
    d->AddConvexPolyFilled(points,count,fill);d->AddPolyline(points,count,line,1.0f,ImDrawFlags_Closed);
}
inline float Motion(ImGuiID id,float target,float rate=12){
    auto*state=ImGui::GetStateStorage();const float before=state->GetFloat(id,target);
    const float next=before+(target-before)*(1-expf(-rate*ImGui::GetIO().DeltaTime));state->SetFloat(id,next);return next;
}
inline void Grid(ImDrawList*, ImVec2, ImVec2, float = 40) {}
inline void Planet(ImDrawList* d, ImVec2 c, float r, float scale = 1) {
    d->AddCircleFilled(c,r,Ink(0x102C3A),96);
    d->AddCircle(c,r,Ink(0x3F8497),96,1.4f*scale);
    d->AddEllipse(c,{r,r*.32f},Ink(0x397081),0,96,scale);
    d->AddEllipse(c,{r*.40f,r},Ink(0x397081),.35f,96,scale);
    d->AddCircle(c,r+12*scale,Ink(0x274C5D),96,scale);
    d->AddLine({c.x-r-24*scale,c.y},{c.x+r+24*scale,c.y},Ink(0x427588),scale);
    d->AddLine({c.x,c.y-r-24*scale},{c.x,c.y+r+24*scale},Ink(0x427588),scale);
    const ImVec2 p{c.x-r*.42f,c.y-r*.28f};
    d->AddCircleFilled(p,3.5f*scale,Ink(0xA4F4F0));
    d->AddCircle(p,9*scale,Ink(0x75DAE2,130),24,scale);
    d->AddLine(p,{p.x+30*scale,p.y-20*scale},Ink(0x75DAE2),scale);
    d->AddLine({p.x+30*scale,p.y-20*scale},{p.x+65*scale,p.y-20*scale},Ink(0x75DAE2),scale);
}
inline void ApplyWidgets() {
    auto& st=ImGui::GetStyle();
    st.WindowRounding=16;st.ChildRounding=14;st.FrameRounding=8;st.PopupRounding=10;
    st.GrabRounding=8;st.TabRounding=14;st.ScrollbarRounding=8;
    st.AntiAliasedFill=true;st.AntiAliasedLines=true;
    st.FrameBorderSize=0;st.TabBorderSize=0;st.SeparatorTextBorderSize=1;
    st.TabBarBorderSize=0;st.TabBarOverlineSize=0;st.PopupBorderSize=1;
    auto*c=st.Colors;
    c[ImGuiCol_Text]=Rgb(0x1C1C1A);c[ImGuiCol_TextDisabled]=Rgb(0x6F6D66);
    c[ImGuiCol_WindowBg]=Rgb(0xF4F3EF);c[ImGuiCol_ChildBg]=Rgb(0xFDFCF9,.5f);
    c[ImGuiCol_PopupBg]=Rgb(0xFDFCF9,.99f);c[ImGuiCol_Border]=Rgb(0xE2E0DA);
    c[ImGuiCol_FrameBg]=Rgb(0xFDFCF9,.95f);c[ImGuiCol_FrameBgHovered]=Rgb(0xE5F2F0);
    c[ImGuiCol_FrameBgActive]=Rgb(0xD5EBE7);c[ImGuiCol_CheckMark]=Rgb(0x127A72);
    c[ImGuiCol_SliderGrab]=Rgb(0x65A69F);c[ImGuiCol_SliderGrabActive]=Rgb(0x0E655F);
    c[ImGuiCol_Button]=Rgb(0xEEEEE8);c[ImGuiCol_ButtonHovered]=Rgb(0xE5F2F0);
    c[ImGuiCol_ButtonActive]=Rgb(0xC8E4DE);c[ImGuiCol_Header]=Rgb(0xE5F2F0,.8f);
    c[ImGuiCol_HeaderHovered]=Rgb(0xDBEEEA);c[ImGuiCol_HeaderActive]=Rgb(0xC8E4DE);
    c[ImGuiCol_Tab]=Rgb(0xF4F3EF);c[ImGuiCol_TabHovered]=Rgb(0xE5F2F0);
    c[ImGuiCol_TabSelected]=Rgb(0x127A72);c[ImGuiCol_TabSelectedOverline]=Rgb(0x127A72,0);
    c[ImGuiCol_TabDimmed]=Rgb(0xF4F3EF);c[ImGuiCol_TabDimmedSelected]=Rgb(0x127A72);
    c[ImGuiCol_TableHeaderBg]=Rgb(0xEEEDE7);c[ImGuiCol_TableBorderStrong]=Rgb(0xE2E0DA);
    c[ImGuiCol_TableBorderLight]=Rgb(0xE2E0DA,.6f);c[ImGuiCol_TableRowBg]=Rgb(0xF4F3EF,.25f);
    c[ImGuiCol_TableRowBgAlt]=Rgb(0xF8F7F3,.45f);
    c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgActive]=c[ImGuiCol_TitleBgCollapsed]=Rgb(0xF4F3EF);
    c[ImGuiCol_Separator]=Rgb(0xE2E0DA);c[ImGuiCol_SeparatorHovered]=Rgb(0x65A69F);
    c[ImGuiCol_SeparatorActive]=Rgb(0x127A72);c[ImGuiCol_TextSelectedBg]=Rgb(0xB8DCD5,.7f);
    c[ImGuiCol_NavCursor]=Rgb(0x127A72);c[ImGuiCol_ScrollbarBg]=Rgb(0xF4F3EF,.5f);
    c[ImGuiCol_ScrollbarGrab]=Rgb(0xD3D4CD);c[ImGuiCol_ScrollbarGrabHovered]=Rgb(0x9DBAB1);
    c[ImGuiCol_ScrollbarGrabActive]=Rgb(0x127A72);
}
inline bool MenuHeader() {
    const auto p=ImGui::GetCursorScreenPos();auto*d=ImGui::GetWindowDrawList();
    const float w=ImGui::GetContentRegionAvail().x;
    d->AddText({p.x+2,p.y+10},Ink(0x1F2A28),"离线宇宙 · 菜单");
    ImGui::SetCursorScreenPos({p.x+w-28,p.y+3});
    ImGui::PushStyleColor(ImGuiCol_Button,Rgb(0xFAFAF7,0));
    const bool close=ImGui::Button("×##close",{28,28});ImGui::PopStyleColor();
    d->AddLine({p.x,p.y+38},{p.x+w,p.y+38},Ink(0x127A72,30));
    ImGui::SetCursorScreenPos({p.x,p.y+47});return !close;
}
}
