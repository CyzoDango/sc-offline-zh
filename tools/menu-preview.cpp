// Development-only visual preview. Reuses the real menu shell and widgets, with
// sample tab callbacks in place of game services. Never loads or starts the game.
#include "../src/menu.cpp"
#include "../src/builtins/crew_ui.cpp"
#include "../src/builtins/contracts_ui.cpp"
#include <cstdarg>

void Log(const char* fmt,...) { va_list a;va_start(a,fmt);vprintf(fmt,a);va_end(a);puts(""); }
static char previewDataDir[MAX_PATH]={};
bool DataFilePath(char* out,DWORD n,const char* file) {if(!previewDataDir[0])return false;return snprintf(out,n,"%s\\%s",previewDataDir,file)>0;}
void Menu_GetStatus(char* out,size_t n) { snprintf(out,n,"界面预览 · 列表为样例数据，不连接游戏。\n中文搜索、生成操作请在实际游戏中测试。"); }

// Preview-only stand-ins: no entity system, hooks or game launch exist here.
static unsigned long long previewTarget=2000002;
int g_npcPick=0;
bool NpcPicker(){const char* labels[]={"帝国海军飞行员（样例）","安保炮手（样例）"};ImGui::SetNextItemWidth(-1);ImGui::Combo("##npc-example",&g_npcPick,labels,2);return true;}
void ProcessCrew(DWORD){}
bool Menu_SeatControlAvailable(){return true;}
unsigned long long Menu_CrewTargetShip(){return previewTarget;}
void Menu_TargetSpawnedShip(unsigned long long id){previewTarget=id;}
void Menu_TargetShipImIn(){previewTarget=2000002;}
int Menu_GetSpawnedShips(MenuSpawnedShip* out,int max,bool){
    MenuSpawnedShip rows[]={{2000001,"RSI_Bengal_PU_AI_UEE",3,0,1,false},{2000002,"AEGS_Javelin",3,0,1,false},{2000003,"RSI_Polaris",3,0,1,false}};
    const int n=(std::min)(max,3);for(int i=0;i<n;++i)out[i]=rows[i];return n;
}
int Menu_GetSeats(MenuSeat* out,int max,char* name,size_t size){
    strncpy_s(name,size,previewTarget==2000001?"RSI_Bengal_PU_AI_UEE":previewTarget==2000002?"AEGS_Javelin":"RSI_Polaris",_TRUNCATE);
    MenuSeat rows[]={{"SCItem_Seat_Pilot",1000,SeatState_Empty,101,1},{"SCItem_Seat_CoPilot",1000,SeatState_Empty,102,1},{"Turret_Gunner_Left",0,SeatState_Npc,103,1},{"Turret_Gunner_Right",0,SeatState_Empty,104,0}};
    const int n=(std::min)(max,4);for(int i=0;i<n;++i)out[i]=rows[i];return n;
}
bool Menu_CrewFillBusy(int* done,int* total){*done=*total=0;return false;}
void Menu_RequestPilot(bool){} void Menu_RequestSit(unsigned long long,bool){}
void Menu_RequestStandUp(unsigned long long){} void Menu_RequestKick(unsigned long long){}
void Menu_RequestAddCrew(unsigned long long,int){} void Menu_RequestFillCrew(int){}
void Menu_RequestStandAll(){} void Menu_RequestClearCrew(){}
ContractsStatus ReadContractsStatus(){ContractsStatus out;out.known=2390;out.supported=120;out.categories[0]=120;out.categories[1]=60;out.categories[2]=30;out.categories[3]=20;out.categories[4]=10;out.listed=24;return out;}
void ReadMissionScriptStatus(int& files,bool& registered){files=229;registered=true;}
void RequestContractOffers(int){}

namespace sco::ui {
std::vector<TabInfo> Tabs() {
    return {{"preview.player","玩家","","preview",0},{"preview.items","物品","","preview",1},
        {"preview.travel","旅行","","preview",2},{"preview.ships","载具","","preview",3},
        {"preview.crew","船员","","preview",4},{"preview.npc","角色","","preview",5},
        {"preview.build","建造","","preview",6},{"preview.contracts","任务","","preview",6},{"preview.multi","多人","","preview",7},
        {"preview.s42","42 中队","","preview",8}};
}
size_t DrawOverlays(void*) { return 0; }
Result DrawTab(const char* id,void*) {
    if(!strcmp(id,"preview.player")){
        SectionHeading("移动控制");static bool clip=false,god=false,ammo=false;
        ImGui::Checkbox("穿墙",&clip);ImGui::SameLine();static float speed=30;
        ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##speed",&speed,1,500,"速度 %.0f");
        Hint("F7 记下你的位置，F8 将你传送回去。\n旅行页面提供目的地和已保存的位置。");
        SectionHeading("个人防护");ImGui::Checkbox("无敌模式",&god);ImGui::SameLine();ImGui::Checkbox("无限弹药",&ammo);
        Hint("穿墙、无敌和无限弹药由 creative 插件提供。");
        SectionHeading("装备配置");static char query[96]={};SearchBox("##equip","搜索装备 / 原始编号",query,sizeof(query));
        const char* labels[]={"基底服：默认","头盔：无","躯干：奥德赛 装甲","手臂：无","腿部：无","背包：无","主武器：无","副武器：无"};
        const float gearWidth=Columns(2);
        for(int i=0;i<8;++i){ImGui::PushID(i);ImGui::SetNextItemWidth(gearWidth);ImGui::BeginDisabled();if(ImGui::BeginCombo("##gear",labels[i]))ImGui::EndCombo();ImGui::EndDisabled();ImGui::PopID();if(i%2==0)ImGui::SameLine();}
        ImGui::BeginDisabled();PrimaryButton("穿上这套装备");ImGui::EndDisabled();
    }else if(!strcmp(id,"preview.ships")){
        SectionHeading("舰队部署");static char filter[96]={};SearchBox("##ships","搜索飞船 / 中文名称 / 型号",filter,sizeof(filter));
        if(ImGui::BeginTable("##ships",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH,ImVec2(0,235))){
            ImGui::TableSetupColumn("飞船");ImGui::TableSetupColumn("尺寸",ImGuiTableColumnFlags_WidthFixed,55);ImGui::TableSetupColumn("长度",ImGuiTableColumnFlags_WidthFixed,70);ImGui::TableHeadersRow();
            const char* names[]={"RSI 孟加拉母舰","剜度 重拳驱逐舰","圣盾 标枪","圣盾 伊德里斯-M","RSI 北极星","德雷克 铁甲","起源 890 跃动"};
            const char* lengths[]={"980 m","425 m","345 m","242 m","155 m","128 m","210 m"};
            for(int i=0;i<7;++i){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Selectable(names[i],i==4,ImGuiSelectableFlags_SpanAllColumns);ImGui::TableNextColumn();ImGui::TextUnformatted(i<5?"6":"5");ImGui::TableNextColumn();ImGui::TextUnformatted(lengths[i]);}ImGui::EndTable();
        }
        SectionHeading("生成配置");const char* labels[]={"飞船版本：原版","敌我关系：友方","船员：NPC 船员","生成方向：上方"};
        for(int i=0;i<4;++i){ImGui::PushID(i);ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##opt",labels[i])){ImGui::Selectable(labels[i],true);ImGui::EndCombo();}ImGui::PopID();}
        static int dist=100,count=1;ImGui::SetNextItemWidth(-1);ImGui::SliderInt("##dist",&dist,20,2000,"距离 %d 米");ImGui::SetNextItemWidth(-1);ImGui::SliderInt("##count",&count,1,20,"%d 艘");
        ImGui::BeginDisabled();PrimaryButton("生成 RSI 北极星 · 原版");ImGui::EndDisabled();
    }else if(!strcmp(id,"preview.crew")){DrawCrewTab(nullptr,nullptr);
    }else if(!strcmp(id,"preview.contracts")){DrawContractsTab(nullptr,nullptr);
    }else{SectionHeading("样式检查");Hint("这个独立窗口仅供界面检查，实际功能仍由游戏内插件绘制。");}
    return Result::Ok;
}
}

static LRESULT CALLBACK PreviewProc(HWND w,UINT m,WPARAM a,LPARAM b){
    if(m==WM_CLOSE||m==WM_DESTROY){PostQuitMessage(0);return 0;}
    return MenuWndProc(w,m,a,b);
}
int main(int argc,char** argv){
    if(argc>1){strncpy_s(previewDataDir,argv[1],_TRUNCATE);CreateDirectoryA(previewDataDir,nullptr);}
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSW wc{};wc.lpfnWndProc=PreviewProc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"sco-menu-theme-preview";RegisterClassW(&wc);
    RECT rect{0,0,kMenuW,kMenuH};AdjustWindowRect(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
    g_wnd=CreateWindowW(wc.lpszClassName,L"sc-offline zh.7 - 菜单样式预览",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,140,70,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!g_wnd||!CreateDevice())return 1;
    ReadMenuPosition();if(g_positionKnown)SetWindowPos(g_wnd,nullptr,g_savedPosition.x,g_savedPosition.y,0,0,SWP_NOSIZE|SWP_NOZORDER);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ApplyTheme();LoadFonts();
    ImGui_ImplWin32_Init(g_wnd);ImGui_ImplDX11_Init(g_device,g_context);ShowWindow(g_wnd,SW_SHOW);
    bool run=true;while(run){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)run=false;TranslateMessage(&msg);DispatchMessageW(&msg);}if(!run)break;
        ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();run=DrawMenu();ImGui::Render();
        const float bg[]={.03f,.06f,.09f,1};g_context->OMSetRenderTargets(1,&g_rtv,nullptr);g_context->ClearRenderTargetView(g_rtv,bg);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());g_swap->Present(1,0);
    }
    ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();return 0;
}
