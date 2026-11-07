#include "ui/star_map.h"
#include <functional>
#include "ui/v3_map_model.h"
#include "ui/planet_renderer.h"
#include <cstdio>
#include <set>
#include <cmath>

// ---- 圆形星图无窗口回归（不创建窗口/设备，不启动游戏）----
// 用 ImGui 无后端帧跑 MapWidget::Draw，软件光栅化本帧绘制命令，检查：圆外像素最终全是卡片底色；圆内不被遮罩；圆外点不可选；圆内可选。
namespace circle_check {
struct Px{ImU32 c=0;};
static void Frame(sco_map::MapWidget&m,sco_map::Selection&c,bool&changed,ImVec2 lo,ImVec2 hi){
 ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});
 ImGui::Begin("circle",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
 changed=m.Draw(lo,hi,c,1)||changed;ImGui::End();ImGui::Render();}
static float Edge(ImVec2 a,ImVec2 b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static std::vector<ImU32> Raster(int W,int H){
 std::vector<ImU32> last(size_t(W)*H,0);auto*dd=ImGui::GetDrawData();
 for(auto*list:dd->CmdLists)for(auto&cmd:list->CmdBuffer){if(cmd.UserCallback)continue;
  for(unsigned e=0;e+2<cmd.ElemCount;e+=3){const ImDrawVert*v[3];bool any=false;
   for(int k=0;k<3;++k){v[k]=&list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+e+k]];any|=((v[k]->col>>IM_COL32_A_SHIFT)&255)!=0;}
   if(!any)continue;
   float x0=(std::min)({v[0]->pos.x,v[1]->pos.x,v[2]->pos.x}),x1=(std::max)({v[0]->pos.x,v[1]->pos.x,v[2]->pos.x});
   float y0=(std::min)({v[0]->pos.y,v[1]->pos.y,v[2]->pos.y}),y1=(std::max)({v[0]->pos.y,v[1]->pos.y,v[2]->pos.y});
   x0=(std::max)(x0,cmd.ClipRect.x);y0=(std::max)(y0,cmd.ClipRect.y);x1=(std::min)(x1,cmd.ClipRect.z);y1=(std::min)(y1,cmd.ClipRect.w);
   for(int y=(std::max)(0,int(floorf(y0)));y<(std::min)(H,int(ceilf(y1))+1);++y)for(int x=(std::max)(0,int(floorf(x0)));x<(std::min)(W,int(ceilf(x1))+1);++x){
    const float px=x+.5f,py=y+.5f;if(px<cmd.ClipRect.x||px>=cmd.ClipRect.z||py<cmd.ClipRect.y||py>=cmd.ClipRect.w)continue;
    const float e0=Edge(v[0]->pos,v[1]->pos,px,py),e1=Edge(v[1]->pos,v[2]->pos,px,py),e2=Edge(v[2]->pos,v[0]->pos,px,py);
    if((e0>=0&&e1>=0&&e2>=0)||(e0<=0&&e1<=0&&e2<=0))last[size_t(y)*W+x]=v[0]->col;}}}
 return last;}
}
static void CircleChecks(const std::function<void(bool,const char*)>&test){
 using namespace circle_check;ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,600};io.DeltaTime=1/60.f;
 unsigned char*pixels;int tw,th;io.Fonts->GetTexDataAsRGBA32(&pixels,&tw,&th);
 const ImVec2 lo{20,20},hi{620,520},a{29,70},b{611,451},center{(a.x+b.x)*.5f,(a.y+b.y)*.5f};const float disk=(std::min)(b.x-a.x,b.y-a.y)*.495f;
 const ImU32 card=sco_terminal::Ink(0xFDFCF9);
 test(sco_map::MaskOuterRadius(a,b,center,128)*cosf(3.14159265f/128)>sqrtf((b.x-center.x)*(b.x-center.x)+(b.y-center.y)*(b.y-center.y)),"mask polygon chords lie beyond every rect corner");
 int overflowSeen=0;
 auto raster=[&](sco_map::MapWidget&m,const char*what){sco_map::Selection c;bool ch=false;io.AddMousePosEvent(-100,-100);for(int i=0;i<3;++i)Frame(m,c,ch,lo,hi);
  for(auto&h:m.hits)if(!sco_map::InDisk(h.at,center,disk)&&h.at.x>a.x&&h.at.x<b.x&&h.at.y>a.y&&h.at.y<b.y)++overflowSeen;
  const auto last=Raster(800,600);int outsideBad=0,insideMasked=0,outside=0;
  for(int y=int(a.y);y<int(b.y);++y)for(int x=int(a.x);x<int(b.x);++x){const float dx=x+.5f-center.x,dy=y+.5f-center.y,r=sqrtf(dx*dx+dy*dy);
   const ImU32 col=last[size_t(y)*800+x];if(r>disk+2.5f){++outside;if(col!=card)++outsideBad;}else if(r<disk-2.f&&col==card)++insideMasked;}
  char label[160];std::snprintf(label,sizeof label,"%s: every pixel outside the disk (incl. rect corners) ends as card colour",what);test(outside>0&&outsideBad==0,label);
  if(outsideBad)std::printf("  %s outside pixels not masked: %d/%d\n",what,outsideBad,outside);
  std::snprintf(label,sizeof label,"%s: mask never paints inside the disk",what);test(insideMasked==0,label);};
 for(double yaw:{.35,1.2,2.5,4.0,5.5})for(float z:{1.f,2.3f}){sco_map::MapWidget m;m.level=1;m.yaw=yaw;m.zoom=m.targetZoom=z;raster(m,z>2?"system view zoomed":"system view");}
 for(int body:{0,5,9,12})for(float z:{1.f,2.3f}){sco_map::MapWidget m;m.level=2;m.body=body;m.zoom=m.targetZoom=z;m.lastInput=1e9;raster(m,z>2?"planet view zoomed":"planet view");}
 test(overflowSeen>0,"scenarios really place bodies/labels outside the disk");
 auto click=[&](sco_map::MapWidget&m,ImVec2 p,sco_map::Selection&c){bool ch=false;io.AddMousePosEvent(p.x,p.y);Frame(m,c,ch,lo,hi);io.AddMouseButtonEvent(0,true);Frame(m,c,ch,lo,hi);io.AddMouseButtonEvent(0,false);Frame(m,c,ch,lo,hi);io.AddMousePosEvent(-100,-100);Frame(m,c,ch,lo,hi);return ch;};
 int outsideClicks=0,insideBody=0,insidePlace=0;
 for(double yaw:{.35,1.2,2.5,4.0,5.5}){
  {sco_map::MapWidget m;m.level=1;m.yaw=yaw;m.zoom=m.targetZoom=2.3f;sco_map::Selection c;bool ch=false;io.AddMousePosEvent(-100,-100);Frame(m,c,ch,lo,hi);auto hits=m.hits;
   for(auto&h:hits)if(h.body>=0&&!sco_map::InDisk(h.at,center,disk)&&h.at.x>a.x+1&&h.at.x<b.x-1&&h.at.y>a.y+1&&h.at.y<b.y-1){sco_map::MapWidget t;t.level=1;t.yaw=yaw;t.zoom=t.targetZoom=2.3f;sco_map::Selection tc;
    const bool chg=click(t,h.at,tc);++outsideClicks;test(!chg&&t.level==1,"click on a body outside the disk selects nothing");}}
  {sco_map::MapWidget m;m.level=1;m.yaw=yaw;sco_map::Selection c;bool ch=false;io.AddMousePosEvent(-100,-100);Frame(m,c,ch,lo,hi);auto hits=m.hits;
   for(auto&h:hits)if(h.body>=0&&h.place<0&&sco_map::InDisk(h.at,center,disk-12)){sco_map::MapWidget t;t.level=1;t.yaw=yaw;sco_map::Selection tc;click(t,h.at,tc);++insideBody;
    test(t.level==2&&t.body==h.body,"click on a body inside the disk still opens it");break;}}}
 for(int body:{0,5,9,12})for(float z:{1.f,2.3f}){sco_map::MapWidget m;m.level=2;m.body=body;m.zoom=m.targetZoom=z;m.lastInput=1e9;sco_map::Selection c;bool ch=false;io.AddMousePosEvent(-100,-100);Frame(m,c,ch,lo,hi);auto hits=m.hits;
  for(auto&h:hits)if(h.place>=0){sco_map::MapWidget t;t.level=2;t.body=body;t.zoom=t.targetZoom=z;t.lastInput=1e9;sco_map::Selection tc;const bool in=sco_map::InDisk(h.at,center,disk-2);const bool inRect=h.at.x>a.x+1&&h.at.x<b.x-1&&h.at.y>a.y+1&&h.at.y<b.y-1;
   if(!in&&!inRect)continue;if(!in&&sco_map::InDisk(h.at,center,disk+2))continue;const bool chg=click(t,h.at,tc);
   if(in){++insidePlace;test(chg&&tc.place==h.place,"click on a city/station inside the disk still sets it");}else{++outsideClicks;test(!chg,"click on a city/station outside the disk sets nothing");}}}
 test(outsideClicks>0&&insideBody>0&&insidePlace>0,"inside and outside click scenarios were exercised");
 std::printf("circle checks: outside clicks %d, inside body clicks %d, inside place clicks %d, overflow hits %d\n",outsideClicks,insideBody,insidePlace,overflowSeen);
 ImGui::DestroyContext();}
int main(){
 int checks=0,failed=0;auto test=[&](bool condition,const char*label){++checks;if(!condition){++failed;std::printf("FAIL %s\n",label);}};
 std::set<std::string>places;for(int p=0;p<sco_map::PlaceCount;++p)if(sco_v3::AllowedPlace(p)){places.insert(sco_map::kPlaces[p].key);test(*sco_v3::PlaceEnglish(p)!=0,"English label exists");}
 const char*expected[]={"Stanton1_Lorville","Stanton2_Orison","Stanton3_Area18","Stanton4_NewBabbage","RR_HUR_LEO","RR_CRU_LEO","RR_ARC_LEO","RR_MIC_LEO","GrimHEX"};
 test(places.size()==9,"exactly nine cities/stations");for(auto key:expected)test(places.contains(key),"required place retained");
 test(!sco_v3::AllowedPlace(-1)&&!sco_v3::AllowedPlace(sco_map::PlaceCount),"invalid indices rejected");
 for(double pitch=-1.45;pitch<=1.45;pitch+=.1)for(double yaw=-6.2;yaw<6.3;yaw+=.2)for(double x:{-1.,0.,1.})for(double z:{-1.,0.,1.}){
  const auto p=sco_v3::Project(x,z,yaw,pitch,2.6);test(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.depth)&&p.factor>0&&p.factor<=4.00001,"projection finite and bounded");}
 const auto near=sco_v3::Project(0,1,0,.62,2.6),far=sco_v3::Project(0,-1,0,.62,2.6);
 test(near.factor>far.factor,"perspective near larger");
 const auto above=sco_v3::Project(0,1,0,.62,2.6),below=sco_v3::Project(0,1,0,-.62,2.6);
 test(std::abs(above.y+below.y)<1e-10,"orbit plane can be viewed from below");
 test(sco_v3::Decay(8,.05)<8&&sco_v3::Decay(-8,.05)>-8,"inertia decays toward zero");
 test(sco_v3::Decay(8,-1)==8,"negative frame time not amplified");
 CircleChecks(test);
 std::string error;test(sco_v3::ValidatePlanetShaders(error),"HLSL VS and PS compile for D3D 10+");if(!error.empty())std::printf("%s\n",error.c_str());
 std::printf("V3 UI checks: %d, failed: %d; no window or game launched\n",checks,failed);return failed?1:0;
}
