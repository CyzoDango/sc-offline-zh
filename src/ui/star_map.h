#pragma once
#include "terminal_theme.h"
#include "planet_renderer.h"
#include "v3_map_model.h"
#include "../zh/star_selection.h"
#include <algorithm>
#include <vector>
#include <cstdio>

namespace sco_map {
inline ImVec2 At(ImVec2 a,float x,float y){return {a.x+x,a.y+y};}
inline ImU32 Blend(unsigned a,unsigned b,double t,double shade=1){
    t=(std::clamp)(t,0.0,1.0);unsigned c[3];
    for(int i=0;i<3;++i){const int shift=16-i*8;c[i]=unsigned((std::clamp)((((a>>shift)&255)*(1-t)+((b>>shift)&255)*t)*shade,0.0,255.0));}
    return IM_COL32(c[0],c[1],c[2],255);
}
inline void Globe(ImDrawList*d,ImVec2 center,float radius,const Camera&cam,const Body&body,bool detailed=true){
    using sco_terminal::Ink;
    for(int i=5;i>0;--i)d->AddCircleFilled(At(center,2,4),radius+i*2.0f,Ink(0xB5A8CE,unsigned(3+i)),72);
    d->AddCircleFilled(center,radius,Ink(0xD8DCE7),96);
    const int rings=detailed?24:8,segments=detailed?88:32;
    const ImVec2 uv=ImGui::GetFontTexUvWhitePixel();
    auto vertex=[&](int ring,int segment,ImVec2&screen,ImU32&color){
        const double r=double(ring)/rings,t=segment*2*Pi/segments,x=r*cos(t),y=r*sin(t);
        Vec world;cam.Pick(x,y,world);screen={center.x+float(x)*radius,center.y-float(y)*radius};
        const double land=sin(world.x*9+sin(world.y*6))+cos(world.y*11-world.z*4)*.55+sin(world.z*14+world.x*5)*.4;
        double mix=(land+.8)/2.3;
        if(body.kind==1)mix=.5+.26*sin(world.z*34+.4*sin(world.x*8));
        if(body.kind==2)mix=(std::max)(mix,pow(fabs(world.z),3)*1.5);
        if(body.kind==3)mix=.45+.14*land+.12*sin(world.x*150)*sin(world.y*150);
        const double depth=sqrt((std::max)(0.0,1-x*x-y*y));
        const double light=(std::max)(0.0,-x*.43+y*.42+depth*.79);
        color=Blend(body.a,body.b,mix,.47+.58*light);
    };
    for(int ring=0;ring<rings;++ring)for(int j=0;j<segments;++j){
        ImVec2 p[4];ImU32 c[4];vertex(ring,j,p[0],c[0]);vertex(ring+1,j,p[1],c[1]);vertex(ring+1,j+1,p[2],c[2]);vertex(ring,j+1,p[3],c[3]);
        const ImDrawIdx index=ImDrawIdx(d->_VtxCurrentIdx);d->PrimReserve(6,4);
        d->PrimWriteIdx(index);d->PrimWriteIdx(ImDrawIdx(index+1));d->PrimWriteIdx(ImDrawIdx(index+2));d->PrimWriteIdx(index);d->PrimWriteIdx(ImDrawIdx(index+2));d->PrimWriteIdx(ImDrawIdx(index+3));
        for(int k=0;k<4;++k)d->PrimWriteVtx(p[k],uv,c[k]);
    }
    d->AddCircle(center,radius,Ink(0x9A93B8,100),96,1);
}

// 圆形星图视口（0.7.7-zh 圆外越界修复）。ImGui 只支持矩形裁剪，所以：
// 1) InDisk：命中/悬停只认圆内；2) MaskOutsideDisk：最后用卡片底色把“圆盘外、矩形内”整片盖住。
// 遮罩内边与 AddCircleFilled(center,disk,...,128) 使用同一组角度 2πi/N 的顶点，两边共用同一条折线，不留缝；
// 外边半径 = 到矩形最远角的距离 / cos(π/N) + 4，保证 N 边形的每条弦都在四个角之外。
inline bool InDisk(ImVec2 p,ImVec2 c,float r){const float x=p.x-c.x,y=p.y-c.y;return x*x+y*y<=r*r;}
inline float MaskOuterRadius(ImVec2 a,ImVec2 b,ImVec2 c,int n){
    float reach=0;const ImVec2 corners[4]={a,b,{a.x,b.y},{b.x,a.y}};
    for(const auto&q:corners)reach=(std::max)(reach,sqrtf((q.x-c.x)*(q.x-c.x)+(q.y-c.y)*(q.y-c.y)));
    return reach/cosf(3.14159265358979f/float(n))+4;
}
inline void MaskOutsideDisk(ImDrawList*d,ImVec2 a,ImVec2 b,ImVec2 c,float r,ImU32 color,int n=128){
    const float outer=MaskOuterRadius(a,b,c,n);const ImVec2 uv=ImGui::GetFontTexUvWhitePixel();
    d->PrimReserve(n*6,n*2);const unsigned base=d->_VtxCurrentIdx;
    for(int i=0;i<n;++i){const float t=6.28318530717958f*float(i)/float(n),x=cosf(t),y=sinf(t);
        d->PrimWriteVtx({c.x+x*r,c.y+y*r},uv,color);d->PrimWriteVtx({c.x+x*outer,c.y+y*outer},uv,color);}
    for(int i=0;i<n;++i){const unsigned in0=base+2*i,out0=in0+1,in1=base+2*((i+1)%n),out1=in1+1;
        d->PrimWriteIdx(ImDrawIdx(in0));d->PrimWriteIdx(ImDrawIdx(out0));d->PrimWriteIdx(ImDrawIdx(out1));
        d->PrimWriteIdx(ImDrawIdx(in0));d->PrimWriteIdx(ImDrawIdx(out1));d->PrimWriteIdx(ImDrawIdx(in1));}
}


struct MapWidget {
    Selection selected{};int level=2,system=0,body=5;Camera camera{};
    float zoom=1,targetZoom=1,velocityX=0,velocityY=0,travel=1;
    double yaw=.35,pitch=sco_v3::PitchDefault,resetAt=-1,resetYaw=0,resetPitch=0,lastInput=0;
    double focusAt=-1,focusLat=0,focusLon=0,fromLat=0,fromLon=0;
    bool initialized=false,dragged=false;
    struct Hit{ImVec2 at,lo,hi;float radius;int body=-1,place=-1,system=-1;};
    std::vector<Hit>hits; // 本帧命中区（成员以便无窗口回归测试读取）
    void Focus(double lat,double lon){fromLat=camera.lat;fromLon=camera.lon;focusLat=lat;focusLon=lon;focusAt=ImGui::GetTime();}
    void Browse(int next,int value=-1){
        level=next;system=0;zoom=targetZoom=1;velocityX=velocityY=0;travel=0;lastInput=ImGui::GetTime();
        if(next==1){resetAt=lastInput;resetYaw=yaw;resetPitch=pitch;}
        if(next==2&&value>=0&&value<16){body=value;Focus(.2,.65);
            for(int i=0;i<PlaceCount;++i)if(kPlaces[i].body==body&&!kPlaces[i].station){Focus(kPlaces[i].lat,kPlaces[i].lon);break;}}
    }
    void Sync(const Selection&s){
        const bool changed=!initialized||selected.place!=s.place||selected.body!=s.body;selected=s;
        if(!initialized){initialized=true;body=s.body>=0&&s.body<16?s.body:5;level=2;camera={s.lat,s.lon};}
        else if(changed&&s.body<16){body=s.body;level=2;Focus(s.lat,s.lon);if(s.place>=0&&kPlaces[s.place].station)targetZoom=(std::min)(targetZoom,1.f);}
    }
    bool Draw(ImVec2 lo,ImVec2 hi,Selection&choice,float scale=1){
        using sco_terminal::Ink;auto*d=ImGui::GetWindowDrawList();auto&io=ImGui::GetIO();
        const float dt=(std::clamp)(io.DeltaTime,0.0001f,.05f);const double now=ImGui::GetTime();
        travel+=(1-travel)*(1-expf(-dt*9));zoom+=(targetZoom-zoom)*(1-expf(-dt*8));
        if(focusAt>=0){const float t=float((now-focusAt)/.2),e=1-powf(1-(std::min)(t,1.f),3);camera.lat=fromLat+(focusLat-fromLat)*e;camera.lon=fromLon+(focusLon-fromLon)*e;if(t>=1)focusAt=-1;}
        if(resetAt>=0){const float t=float((now-resetAt)/.5),e=1-powf(1-(std::min)(t,1.f),3);yaw=resetYaw+(.35-resetYaw)*e;pitch=resetPitch+(sco_v3::PitchDefault-resetPitch)*e;if(t>=1)resetAt=-1;}
        ImGui::PushID("star-map-v3");ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,6*scale);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{9*scale,5*scale});
        ImGui::SetCursorScreenPos(At(lo,15*scale,11*scale));
        if(ImGui::Button("全部星系"))Browse(0);if(level>=1){ImGui::SameLine(0,5*scale);if(ImGui::Button("史坦顿"))Browse(1);}
        if(level==2){ImGui::SameLine(0,5*scale);ImGui::TextColored(sco_terminal::Rgb(0x127A72),"%s",kBodies[body].name);}
        ImGui::SetCursorScreenPos({hi.x-131*scale,lo.y+11*scale});
        if(ImGui::Button("−",{28*scale,28*scale}))targetZoom=(std::max)(.64f,targetZoom/1.2f);ImGui::SameLine(0,5*scale);
        if(ImGui::Button("+",{28*scale,28*scale}))targetZoom=(std::min)(2.3f,targetZoom*1.2f);ImGui::SameLine(0,5*scale);
        if(ImGui::Button("复位",{43*scale,28*scale})){zoom=targetZoom=1;velocityX=velocityY=0;resetAt=now;resetYaw=yaw;resetPitch=pitch;Focus(.2,.65);}
        ImGui::PopStyleVar(2);
        const ImVec2 a=At(lo,9*scale,50*scale),b{hi.x-9*scale,hi.y-69*scale};
        if(b.x<=a.x||b.y<=a.y){ImGui::PopID();return false;}
        const float w=b.x-a.x,h=b.y-a.y;
        ImGui::SetCursorScreenPos(a);ImGui::InvisibleButton("##canvas",{w,h},ImGuiButtonFlags_MouseButtonLeft);
        // 圆心/半径提前计算：鼠标在圆外不算悬停（不误选、不显示手形、不响应滚轮）
        const ImVec2 center{(a.x+b.x)*.5f,(a.y+b.y)*.5f};const float disk=(std::min)(w,h)*.495f;
        const bool hover=ImGui::IsItemHovered()&&InDisk(io.MousePos,center,disk),active=ImGui::IsItemActive();
        if(ImGui::IsItemActivated()){dragged=false;velocityX=velocityY=0;lastInput=now;}
        if(active&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,5)){
            dragged=true;focusAt=resetAt=-1;lastInput=now;
            const double dx=-io.MouseDelta.x*.0065/scale,dy=io.MouseDelta.y*.0055/scale;
            velocityX=float((std::clamp)(dx/dt,-8.,8.));velocityY=float((std::clamp)(dy/dt,-6.,6.));
            if(level==1){yaw+=dx;pitch=(std::clamp)(pitch+dy,sco_v3::PitchMin,sco_v3::PitchMax);}
            else{camera.lon+=dx;camera.lat=(std::clamp)(camera.lat+dy,-1.396,1.396);}
        }else if(!active&&focusAt<0&&resetAt<0){
            velocityX=float(sco_v3::Decay(velocityX,dt));velocityY=float(sco_v3::Decay(velocityY,dt));
            if(level==1){yaw+=velocityX*dt;pitch=(std::clamp)(pitch+velocityY*dt,sco_v3::PitchMin,sco_v3::PitchMax);if(pitch<=sco_v3::PitchMin||pitch>=sco_v3::PitchMax)velocityY=0;}
            else{camera.lon+=velocityX*dt;camera.lat=(std::clamp)(camera.lat+velocityY*dt,-1.396,1.396);if(level==2&&now-lastInput>2.5&&!hover)camera.lon+=dt*.05236;}
        }
        if(hover&&io.MouseWheel){targetZoom=(std::clamp)(targetZoom*expf(io.MouseWheel*.14f),.64f,2.3f);lastInput=now;}
        const bool click=ImGui::IsItemDeactivated()&&!dragged&&ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        if(hover)ImGui::SetMouseCursor(active&&dragged?ImGuiMouseCursor_ResizeAll:ImGuiMouseCursor_Hand);
        d->PushClipRect(a,b,true);d->AddCircleFilled(center,disk,Ink(0x10151D),128);
        for(int i=0;i<135;++i){const double theta=i*2.3999632297,rr=sqrt((i+.5)/135.)*disk*.97;
            d->AddCircleFilled({center.x+float(cos(theta)*rr),center.y+float(sin(theta)*rr)},(i%13==0?1.2f:.65f)*scale,Ink(0xDCE9F2,35+(i*37)%125),8);}
        hits.clear();
        auto globe=[&](int index,ImVec2 at,float r,bool detail){
            if(!sco_v3::Planet(d,at,r,camera.lat,camera.lon,index)){Body fallback{};if(index<16)fallback=kBodies[index];else{fallback.kind=1;fallback.a=0xF6D78C;fallback.b=0xFFF4BE;}Globe(d,at,r,camera,fallback,detail);}
        };
        auto label=[&](ImVec2 at,const char*text,bool selectedLabel,int bi,int pi,float radius){
            // 锚点在圆外：不画标签（否则会被夹到矩形边缘、甚至挤进圆内）；仍压入一条空框命中，保持 hits.back() 约定
            if(!InDisk(at,center,disk)){hits.push_back({at,at,at,radius,bi,pi,-1});return;}
            const auto sz=ImGui::CalcTextSize(text);const float pw=sz.x+14*scale,ph=sz.y+8*scale;
            ImVec2 l{(std::clamp)(at.x-pw*.5f,a.x+3*scale,b.x-pw-3*scale),(std::clamp)(at.y,a.y+2*scale,b.y-ph-2*scale)},r{l.x+pw,l.y+ph};
            d->AddRectFilled(l,r,Ink(selectedLabel?0xE5F2F0:0xF4F3EF,245),5*scale);d->AddRect(l,r,Ink(0xE2E0DA,200),5*scale);
            d->AddText(At(l,7*scale,4*scale),Ink(selectedLabel?0x0E655F:0x1D2125),text);
            hits.push_back({at,l,r,radius,bi,pi,-1});
        };
        auto bodyDot=[&](int bi,ImVec2 at,float r){globe(bi,at,r,false);label(At(at,0,r+5*scale),kBodies[bi].name,selected.body==bi,bi,-1,r+5*scale);hits.back().at=at;};
        if(level==0){globe(16,center,disk*.28f,false);label(At(center,0,disk*.35f),"史坦顿",true,-1,-1,disk*.38f);hits.back().system=0;}
        else if(level==1){
            struct Node{int body;ImVec2 p;float radius;double depth;};std::vector<Node>nodes;
            const int ids[]={0,5,9,12};const double orbits[]={.30,.50,.68,.88},angles[]={.4,2.2,4.1,5.6};
            const double extent=disk*.87*zoom,camD=extent*2.6;
            auto proj=[&](double x,double z){return sco_v3::Project(x,z,yaw,pitch,camD);};
            for(int n=0;n<4;++n){const double orbit=extent*orbits[n];const auto p=proj(cos(angles[n])*orbit,sin(angles[n])*orbit);
                ImVec2 at{center.x+float(p.x),center.y+float(p.y)};nodes.push_back({ids[n],at,float((n==1?16:11)*scale*zoom*p.factor),p.depth});
                int moon=0;for(int m=0;m<16;++m)if(kBodies[m].parent==ids[n]){const double ma=kBodies[m].angle+moon*1.4;const auto q=proj(cos(angles[n])*orbit+cos(ma)*(18+moon*4)*scale,sin(angles[n])*orbit+sin(ma)*(18+moon*4)*scale);
                    nodes.push_back({m,{center.x+float(q.x),center.y+float(q.y)},float(3.5*scale*zoom*q.factor),q.depth});++moon;}}
            auto rings=[&](bool front){for(int n=0;n<4;++n)for(int seg=0;seg<120;++seg){const double t=seg*2*Pi/120,u=(seg+1)*2*Pi/120;const auto p=proj(cos(t)*extent*orbits[n],sin(t)*extent*orbits[n]),q=proj(cos(u)*extent*orbits[n],sin(u)*extent*orbits[n]);if((p.depth>=0)!=front)continue;
                d->AddLine({center.x+float(p.x),center.y+float(p.y)},{center.x+float(q.x),center.y+float(q.y)},Ink(0xD9DDD8,front?82:30),(front?1.1f:.7f)*scale);}};
            rings(false);std::sort(nodes.begin(),nodes.end(),[](const Node&x,const Node&y){return x.depth<y.depth;});
            for(auto&n:nodes)if(n.depth<0){if(n.radius>6*scale)bodyDot(n.body,n.p,n.radius);else{globe(n.body,n.p,n.radius,false);hits.push_back({n.p,n.p,n.p,8*scale,n.body});}}
            globe(16,center,22*scale*zoom,false);rings(true);
            for(auto&n:nodes)if(n.depth>=0){if(n.radius>6*scale)bodyDot(n.body,n.p,n.radius);else{globe(n.body,n.p,n.radius,false);hits.push_back({n.p,n.p,n.p,8*scale,n.body});}}
        }else{
            const float r=disk*.69f*zoom,rr=r*1.25f;auto ring=[&](bool front){for(int n=0;n<120;++n){double t=n*2*Pi/120,u=(n+1)*2*Pi/120;if((sin(t)*cos(pitch)>=0)!=front)continue;
                d->AddLine({center.x+float(cos(t))*rr,center.y+float(sin(t)*sin(pitch))*rr},{center.x+float(cos(u))*rr,center.y+float(sin(u)*sin(pitch))*rr},Ink(0xDDE3DF,front?140:55),(front?1.1f:.7f)*scale);}};
            ring(false);globe(body,center,r,true);ring(true);
            for(int p=0;p<PlaceCount;++p)if(sco_v3::AllowedPlace(p)&&kPlaces[p].body==body){
                const auto&place=kPlaces[p];ImVec2 at{};
                if(place.station){double angle=sco_v3::StationAngle(p);at={center.x+float(cos(angle))*rr,center.y+float(sin(angle)*sin(pitch))*rr};}
                else{const auto pos=camera.Project(Unit(place.lat,place.lon));if(pos.z<.03)continue;at={center.x+float(pos.x)*r,center.y-float(pos.y)*r};}
                const bool chosen=selected.place==p;d->AddCircle(at,(chosen?9:6)*scale,Ink(0x8DD3C6,200),28,1.4f*scale);d->AddCircleFilled(at,2.5f*scale,Ink(0x91D8CD),16);
                label(At(at,0,-31*scale),place.name,chosen,body,p,14*scale);hits.back().at=at;
            }
        }
        Hit*hot=nullptr;float nearest=1e20f;
        for(auto&hit:hits){if(!InDisk(hit.at,center,disk))continue; // 中心在圆外的星球/卫星/地点不可选
            const float dx=io.MousePos.x-hit.at.x,dy=io.MousePos.y-hit.at.y,dist=dx*dx+dy*dy;
            const bool box=io.MousePos.x>=hit.lo.x&&io.MousePos.x<=hit.hi.x&&io.MousePos.y>=hit.lo.y&&io.MousePos.y<=hit.hi.y;
            if((box||dist<=hit.radius*hit.radius)&&dist<nearest){hot=&hit;nearest=dist;}}
        bool changed=false;
        if(hover&&hot){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);if(hot->body>=0&&hot->place<0)ImGui::SetTooltip("%s",kBodies[hot->body].name);}
        if(click&&hover&&hot){lastInput=now;if(hot->place>=0&&sco_v3::AllowedPlace(hot->place)){const auto&p=kPlaces[hot->place];choice={p.body,hot->place,p.lat,p.lon,false};changed=true;}
            else if(hot->system>=0)Browse(1);else if(hot->body>=0)Browse(2,hot->body);}
        // Empty sphere clicks never create a surface selection.
        if(targetZoom<.7f&&level>0)Browse(level-1);
        // 最后绘制：圆外遮罩（卡片底色 kUiCard=253,252,249）+ 抗锯齿圆边；仍在 a..b 矩形裁剪内，碰不到按钮和底部文字
        MaskOutsideDisk(d,a,b,center,disk,Ink(0xFDFCF9));d->AddCircle(center,disk,Ink(0x10151D),128,1.5f*scale);
        d->PopClipRect();
        const float foot=hi.y-62*scale;
        d->AddText({lo.x+18*scale,foot},Ink(0x127A72),"♧ 主城    ◎ 空间站");
        const std::string selection="已选择："+std::string(Name(selected));d->AddText({lo.x+18*scale,foot+20*scale},Ink(0x1C1C1A),selection.c_str());
        d->AddText({lo.x+18*scale,foot+40*scale},Ink(0x9A978E),"拖动旋转 · 滚轮缩放 · 点击城市或空间站图标设为出生点");
        ImGui::PopID();return changed;
    }
};
}
