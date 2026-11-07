// The menu: its own window and D3D11 device, the ImGui frame, and a shell that lists the tabs,
// badges and overlays registered through sco-core's sco.ui service (sco/ui.h). The features'
// pages are drawn by their built-in plugins (src/builtins/*_ui.cpp); this file keeps the window,
// the look, the tab bar, the product's own Menu tab, the status strip and the shared widgets
// (menu_ui.h).
#include "menu.h"
#include "ui/terminal_theme.h"
#include "menu_imgui.h"
#include "menu_ui.h"
#include "common.h"
#include "version.h"
#include "zh/ship_names.h"
#include "zh/content_names.h"
#include <atomic>
#include <algorithm>
#include <climits>
#include <cmath>
#include <type_traits>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "sco/ui.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_internal.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "third_party/imgui/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND                    g_game;
static HWND                    g_wnd;
static ID3D11Device*           g_device;
static ID3D11DeviceContext*    g_context;
static IDXGISwapChain*         g_swap;
static ID3D11RenderTargetView* g_rtv;
static UINT                    g_resizeW, g_resizeH;
static bool g_windowMoving=false,g_positionKnown=false;
static bool g_clipped=false;
static POINT g_savedPosition{};
static void SaveMenuPosition();
static void ResetMenuPosition();

// Fixed CSS logical size. Game resolution never scales the menu.
constexpr int kMenuW = 760, kMenuH = 520;
constexpr float kMenuCornerRadius = 16.0f, kMenuPanelAlpha = 0.90f;
constexpr int kMenuPositionLayout = 2;
static float g_menuDpiScale = 1.0f;
static int g_menuW = kMenuW, g_menuH = kMenuH;
static void ApplyTheme();
static float MenuInset();
static bool g_glass=false;
static float MenuInset(){return g_glass?10.0f*g_menuDpiScale:0.0f;}
static IDCompositionDevice* g_composition=nullptr;
static IDCompositionTarget* g_compTarget=nullptr;
static IDCompositionVisual* g_compVisual=nullptr;
static std::atomic<ULONGLONG> g_openTick{0},g_closeTick{0};
static double g_v3PageSince=0;
// Microsoft YaHei UI: hhea height 2601 / units-per-em 2048. Match CSS em sizing.
constexpr float kFontEmToPixel=2601.0f/2048.0f;
static ImFont* g_menuBold=nullptr;

const std::string& ChineseShipLabel(const char* cls) { return sco_zh::ShipText(cls).label; }
bool MatchesShipFilter(const char* cls, const char* filter) { return sco_zh::ShipMatches(cls, filter); }
static_assert(static_cast<int>(sco_zh::ContentKind::Category) == Name_Category);
const std::string& ChineseName(LocalNameKind kind, const char* raw) { return sco_zh::NameText(static_cast<sco_zh::ContentKind>(kind), raw).label; }
bool MatchesName(LocalNameKind kind, const char* raw, const char* query) { return sco_zh::ContentMatches(static_cast<sco_zh::ContentKind>(kind), raw, query); }
std::string ChinesePlace(const char* name, const char* entity) { return sco_zh::PlaceLabel(name, entity); }

static void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back))) && back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

static void ReleaseRenderTarget() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static bool CreateLegacyDevice() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_wnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got, &g_context);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

// Documented DirectComposition path: per-pixel premultiplied alpha, no desktop capture.
static bool CreateDevice(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory2* factory=nullptr;IDXGISwapChain1* swap=nullptr;
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};D3D_FEATURE_LEVEL feature;
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=g_menuW;desc.Height=g_menuH;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;desc.Scaling=DXGI_SCALING_STRETCH;
    bool ok=SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,2,D3D11_SDK_VERSION,&g_device,&feature,&g_context))&&
        SUCCEEDED(g_device->QueryInterface(IID_PPV_ARGS(&dxgi)))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))&&
        SUCCEEDED(factory->CreateSwapChainForComposition(g_device,&desc,nullptr,&swap))&&
        SUCCEEDED(DCompositionCreateDevice(dxgi,IID_PPV_ARGS(&g_composition)))&&SUCCEEDED(g_composition->CreateTargetForHwnd(g_wnd,TRUE,&g_compTarget))&&
        SUCCEEDED(g_composition->CreateVisual(&g_compVisual))&&SUCCEEDED(g_compVisual->SetContent(swap))&&SUCCEEDED(g_compTarget->SetRoot(g_compVisual))&&SUCCEEDED(g_composition->Commit());
    if(factory)factory->Release();if(adapter)adapter->Release();if(dxgi)dxgi->Release();
    if(ok){g_swap=swap;CreateRenderTarget();if(g_rtv){g_glass=true;
        // Plain per-pixel transparency: a system backdrop would insert another
        // material behind the panel instead of showing the game through it.
        Log("[ui-v3] composition alpha enabled; panel alpha=0.90; no system backdrop or blur (appearance requires user verification)");return true;}}
    ReleaseRenderTarget();if(swap)swap->Release();g_swap=nullptr;
    if(g_compVisual){g_compVisual->Release();g_compVisual=nullptr;}if(g_compTarget){g_compTarget->Release();g_compTarget=nullptr;}if(g_composition){g_composition->Release();g_composition=nullptr;}
    if(g_context){g_context->Release();g_context=nullptr;}if(g_device){g_device->Release();g_device=nullptr;}
    SetWindowLongPtrW(g_wnd,GWL_EXSTYLE,GetWindowLongPtrW(g_wnd,GWL_EXSTYLE)&~WS_EX_NOREDIRECTIONBITMAP);
    Log("[ui-v3] composition unavailable; opaque readable fallback");return CreateLegacyDevice();
}

static bool GameThreadBuilding();

static LRESULT CALLBACK MenuWndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    // Native hit testing moves the real menu window, not just the ImGui child.
    if(msg==WM_NCHITTEST){
        POINT p{static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};ScreenToClient(h,&p);
        RECT client{};GetClientRect(h,&client);
        const float inset=MenuInset();
        if(g_glass&&(p.x<inset||p.x>=client.right-inset||p.y<inset||p.y>=client.bottom-inset))return HTTRANSPARENT;
        if(p.y>=inset&&p.y<inset+42*g_menuDpiScale&&p.x>=inset+8*g_menuDpiScale&&p.x<client.right-inset-46*g_menuDpiScale)return HTCAPTION;
    }
    if(msg==WM_ENTERSIZEMOVE){g_windowMoving=true;g_clipped=false;ClipCursor(nullptr);return 0;}
    if(msg==WM_EXITSIZEMOVE){g_windowMoving=false;SaveMenuPosition();return 0;}
    // A message sent to the window while the game thread builds a frame doesn't reach ImGui: the
    // game thread has it until the frame is built.
    if (!GameThreadBuilding() && ImGui_ImplWin32_WndProcHandler(h, msg, w, l)) return 1;
    switch (msg) {
    case WM_APP+74: ResetMenuPosition(); return 0;
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { g_resizeW = LOWORD(l); g_resizeH = HIWORD(l); }
        return 0;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_SYSCOMMAND:
        if ((w & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    }
    return DefWindowProcW(h, msg, w, l);
}

static bool OurProcessHasFocus() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static RECT g_clipRect = {};   // the rect last clipped to

static void KeepCursorInMenu() {
    if (g_windowMoving || GetForegroundWindow() != g_wnd) {
        if (g_clipped) { ClipCursor(nullptr); g_clipped = false; }
        return;
    }
    RECT r = {};
    GetWindowRect(g_wnd, &r);
    const int inset=static_cast<int>(std::lround(MenuInset()));InflateRect(&r,-inset,-inset);
    // ClipCursor is a synchronous cross-process input call: only when the window moved or resized.
    if (g_clipped && EqualRect(&r, &g_clipRect)) return;
    g_clipRect = r;
    ClipCursor(&r);
    g_clipped = true;
}

static RECT MenuGameArea() {
    RECT area{};
    POINT origin{};
    if (g_game && GetClientRect(g_game, &area) && area.right > 0 && area.bottom > 0 &&
        ClientToScreen(g_game, &origin)) {
        OffsetRect(&area, origin.x, origin.y);
        return area;
    }
    MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromWindow(g_game, MONITOR_DEFAULTTOPRIMARY), &monitor))
        return monitor.rcWork;
    return {0, 0, 1024, 764};
}

static void RefreshMenuSize() {
    const UINT dpi=g_game?GetDpiForWindow(g_game):96;
    const float scale=(dpi?dpi:96)/96.0f;
    const float previous=g_menuDpiScale;g_menuDpiScale=scale;
    const int border=static_cast<int>(std::lround(MenuInset()*2));
    const int width=static_cast<int>(std::lround(kMenuW*scale))+border;
    const int height=static_cast<int>(std::lround(kMenuH*scale))+border;
    if(width!=g_menuW||height!=g_menuH){g_menuW=width;g_menuH=height;g_positionKnown=false;}
    if(previous!=scale&&ImGui::GetCurrentContext())ApplyTheme();
}

static POINT CenteredMenuPosition() {
    const RECT game = MenuGameArea();
    return {game.left + (game.right - game.left - g_menuW) / 2,
            game.top + (game.bottom - game.top - g_menuH) / 2};
}

static void ApplyMenuCornerMask() {
    if(g_glass){SetWindowRgn(g_wnd,nullptr,TRUE);return;}
    const int diameter=static_cast<int>(std::lround(kMenuCornerRadius*g_menuDpiScale*2));
    HRGN region=CreateRoundRectRgn(0,0,g_menuW+1,g_menuH+1,diameter,diameter);
    if(region&&!SetWindowRgn(g_wnd,region,TRUE))DeleteObject(region);
}

static POINT VisibleMenuPosition(POINT position){
    RECT rect{position.x,position.y,position.x+g_menuW,position.y+g_menuH};
    MONITORINFO monitor{sizeof(monitor)};
    if(GetMonitorInfoW(MonitorFromRect(&rect,MONITOR_DEFAULTTONEAREST),&monitor)){
        const auto& area=monitor.rcWork;
        position.x=(std::clamp)(position.x,area.left,(std::max)(area.left,area.right-g_menuW));
        position.y=(std::clamp)(position.y,area.top,(std::max)(area.top,area.bottom-g_menuH));
    }
    return position;
}
static void SaveMenuPosition(){
    RECT rect{};if(!g_wnd||!GetWindowRect(g_wnd,&rect))return;
    g_savedPosition=VisibleMenuPosition({rect.left,rect.top});g_positionKnown=true;
    char path[MAX_PATH];if(!DataFilePath(path,sizeof(path),"menu-window.ini"))return;
    char x[24],y[24];snprintf(x,sizeof(x),"%ld",g_savedPosition.x);snprintf(y,sizeof(y),"%ld",g_savedPosition.y);
    char layout[16];snprintf(layout,sizeof(layout),"%d",kMenuPositionLayout);
    if(!WritePrivateProfileStringA("position","x",x,path)||!WritePrivateProfileStringA("position","y",y,path)||
       !WritePrivateProfileStringA("position","layout",layout,path))
        Log("[menu] position could not be saved; keeping it for this session");
}
static void ReadMenuPosition(){
    char path[MAX_PATH];if(!DataFilePath(path,sizeof(path),"menu-window.ini"))return;
    if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)return;
    if(GetPrivateProfileIntA("position","layout",0,path)!=kMenuPositionLayout)return;
    const int missing=INT_MIN;
    const int x=GetPrivateProfileIntA("position","x",missing,path),y=GetPrivateProfileIntA("position","y",missing,path);
    if(x==missing||y==missing)return;
    g_savedPosition=VisibleMenuPosition({x,y});g_positionKnown=true;
}
static void ResetMenuPosition(){
    RefreshMenuSize();
    g_savedPosition=VisibleMenuPosition(CenteredMenuPosition());g_positionKnown=true;
    SetWindowPos(g_wnd,HWND_TOPMOST,g_savedPosition.x,g_savedPosition.y,g_menuW,g_menuH,SWP_NOACTIVATE);
    ApplyMenuCornerMask();
    SaveMenuPosition();
}
static void ShowMenu(bool show) {
    if (show) {
        g_openTick=GetTickCount64();g_closeTick=0;
        RefreshMenuSize();
        if(!g_positionKnown){g_savedPosition=CenteredMenuPosition();g_positionKnown=true;}
        g_savedPosition=VisibleMenuPosition(g_savedPosition);
        SetWindowPos(g_wnd,HWND_TOPMOST,g_savedPosition.x,g_savedPosition.y,g_menuW,g_menuH,SWP_SHOWWINDOW);
        ApplyMenuCornerMask();
        const RECT game=MenuGameArea();
        Log("[menu-style-zh12] logical-panel=760x520 host=%dx%d game=%ldx%ld dpi-scale=%.3f inset=%.1f position=%ld,%ld radius=16 alpha=%s",
            g_menuW,g_menuH,game.right-game.left,game.bottom-game.top,g_menuDpiScale,MenuInset(),
            g_savedPosition.x,g_savedPosition.y,g_glass?"0.90":"1.00 (legacy fallback)");
        SetForegroundWindow(g_wnd);
        SetCursorPos(g_savedPosition.x+g_menuW/2,g_savedPosition.y+g_menuH/2);
        KeepCursorInMenu();
    } else {
        SaveMenuPosition();ClipCursor(nullptr);g_clipped=false;
        ShowWindow(g_wnd,SW_HIDE);SetForegroundWindow(g_game);
    }
}

static bool ContainsNoCase(const char* s, const char* needle, size_t n) {
    for (; *s; ++s) {
        size_t i = 0;
        while (i < n && s[i] && tolower(static_cast<unsigned char>(s[i])) == tolower(static_cast<unsigned char>(needle[i]))) ++i;
        if (i == n) return true;
    }
    return false;
}

bool MatchesFilter(const char* name, const char* filter) {
    for (const char* p = filter; *p; ) {
        p += strspn(p, " _");
        const size_t n = strcspn(p, " _");
        if (n && !ContainsNoCase(name, p, n)) return false;
        p += n;
    }
    return true;
}

// =============================================================================================
// Cockpit terminal: midnight navy, instrument cyan and restrained metallic outlines.
// Optional user artwork is dimmed beneath the same legible controls.
// =============================================================================================

static ImVec4 Hex(uint32_t rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}

static const ImVec4 kSoil      = Hex(0xF4F3EF);   // deepest green, under the image
static const ImVec4 kPanel     = Hex(0xFFFFFF);   // inputs, lists, status strip
static const ImVec4 kPanelUp   = Hex(0xE5F2F0);   // hover
static const ImVec4 kLine      = Hex(0xDED9E8);   // dividers, scrollbars
static const ImVec4 kText      = Hex(0x1C1C1A);
const ImVec4 kMuted     = Hex(0x6F6D66);
const ImVec4 kLeaf      = Hex(0x127A72);   // accent
const ImVec4 kLeafDeep  = Hex(0x127A72);

constexpr float kBodySize    = 13.0f;
constexpr float kHeadingSize = 12.0f;

static void ApplyTheme() {
    auto&st=ImGui::GetStyle();st=ImGuiStyle();sco_terminal::ApplyWidgets();
    st.WindowPadding={0,0};st.FramePadding={12,(34-13*kFontEmToPixel)*.5f};st.ItemSpacing={8,4};st.ItemInnerSpacing={6,4};st.CellPadding={8,0};
    st.DisplaySafeAreaPadding={10,10};st.DisplayWindowPadding={12,12};st.ScrollbarSize=8;st.WindowRounding=16;st.ChildRounding=10;st.FrameRounding=10;st.PopupRounding=10;
    st.GrabRounding=7;st.TabRounding=14;st.ScrollbarRounding=8;st.WindowBorderSize=0;st.ChildBorderSize=0;
    st.FrameBorderSize=1;st.PopupBorderSize=1;st.SeparatorTextBorderSize=0;st.FramePadding={12,(34-13*kFontEmToPixel)*.5f};
    auto*c=st.Colors;c[ImGuiCol_WindowBg]=c[ImGuiCol_ChildBg]={0,0,0,0};
    c[ImGuiCol_FrameBg]=Hex(0xFFFFFF,.72f);c[ImGuiCol_FrameBgHovered]=Hex(0xFFFFFF,.85f);c[ImGuiCol_FrameBgActive]=Hex(0xFFFFFF,.94f);
    c[ImGuiCol_Border]=Hex(0x127A72,.20f);c[ImGuiCol_PopupBg]=Hex(0xFAFAF7,.98f);
    c[ImGuiCol_Header]=Hex(0x127A72,.08f);c[ImGuiCol_HeaderHovered]=Hex(0x127A72,.08f);c[ImGuiCol_HeaderActive]=Hex(0x127A72,.14f);
    c[ImGuiCol_Button]=Hex(0x127A72);c[ImGuiCol_ButtonHovered]=Hex(0x0E6B64);c[ImGuiCol_ButtonActive]=Hex(0x0E655F);
    c[ImGuiCol_ScrollbarBg]={0,0,0,0};c[ImGuiCol_ScrollbarGrab]=Hex(0x127A72,.30f);c[ImGuiCol_ScrollbarGrabHovered]=Hex(0x127A72,.45f);c[ImGuiCol_ScrollbarGrabActive]=Hex(0x127A72,.60f);
    c[ImGuiCol_TableRowBg]=c[ImGuiCol_TableRowBgAlt]={0,0,0,0};
    st.ScaleAllSizes(g_menuDpiScale);st.FontSizeBase=13*g_menuDpiScale*kFontEmToPixel;st.FontScaleMain=1;
}

// Bahnschrift (DIN-style, ships with Windows 10+), then Segoe UI, then ImGui's own font.
static void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    char dir[MAX_PATH] = "C:\\Windows";
    GetWindowsDirectoryA(dir, sizeof(dir));
    static const char* const kFonts[] = { "msyh.ttc", "simhei.ttf", "segoeui.ttf" };
    ImFont* font = nullptr;
    for (const char* name : kFonts) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\Fonts\\%s", dir, name);
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
        ImFontConfig config;config.FontNo=!strcmp(name,"msyh.ttc")?1:0;
        if ((font = io.Fonts->AddFontFromFileTTF(path, kBodySize*kFontEmToPixel,&config)) != nullptr) break;
    }
    ImGuiStyle& st = ImGui::GetStyle();
    char boldPath[MAX_PATH];snprintf(boldPath,sizeof(boldPath),"%s\\Fonts\\msyhbd.ttc",dir);
    if(GetFileAttributesA(boldPath)!=INVALID_FILE_ATTRIBUTES){ImFontConfig config;config.FontNo=1;g_menuBold=io.Fonts->AddFontFromFileTTF(boldPath,kBodySize*kFontEmToPixel,&config);}
    st.FontSizeBase = kBodySize*g_menuDpiScale*kFontEmToPixel;
    st.FontScaleMain = 1.0f;
}

// --- background image: data\menu_background.png / .jpg, decoded with Windows' own WIC ---------

static ID3D11ShaderResourceView* g_bgSrv = nullptr;
static UINT  g_bgW = 0, g_bgH = 0;
static bool  g_bgShow = true;
static int   g_bgDarkness = 74;       // percent of the green wash over the image
static float g_bgPosition = 0.5f;     // which part of a wide image shows in the tall menu
static char  g_bgPath[MAX_PATH] = "";

static bool DecodeImage(const char* file, std::vector<uint8_t>& pixels, UINT& w, UINT& h) {
    wchar_t wide[MAX_PATH];
    if (!MultiByteToWideChar(CP_ACP, 0, file, -1, wide, MAX_PATH)) return false;
    IWICImagingFactory*    factory = nullptr;
    IWICBitmapDecoder*     decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter*   rgba = nullptr;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))
           && SUCCEEDED(factory->CreateDecoderFromFilename(wide, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))
           && SUCCEEDED(decoder->GetFrame(0, &frame))
           && SUCCEEDED(factory->CreateFormatConverter(&rgba))
           && SUCCEEDED(rgba->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))
           && SUCCEEDED(rgba->GetSize(&w, &h))
           && w > 0 && h > 0 && w <= 8192 && h <= 8192;
    if (ok) {
        pixels.resize(static_cast<size_t>(w) * h * 4);
        ok = SUCCEEDED(rgba->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size()), pixels.data()));
    }
    if (rgba) rgba->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    return ok;
}

static void LoadBackground() {
    static const char* const kNames[] = { "menu_background.png", "menu_background.jpg", "menu_background.jpeg" };
    for (const char* name : kNames) {
        char path[MAX_PATH];
        if (!DataFilePath(path, sizeof(path), name)) return;
        if (!g_bgPath[0]) strcpy_s(g_bgPath, path);           // shown in the Menu tab if nothing loads
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
        std::vector<uint8_t> pixels;
        UINT w = 0, h = 0;
        if (!DecodeImage(path, pixels, w, h)) { Log("[menu] couldn't read %s", path); continue; }
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA init = { pixels.data(), w * 4, 0 };
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(g_device->CreateTexture2D(&desc, &init, &tex)) && tex) {
            g_device->CreateShaderResourceView(tex, nullptr, &g_bgSrv);
            tex->Release();
        }
        if (g_bgSrv) {
            g_bgW = w;
            g_bgH = h;
            strcpy_s(g_bgPath, path);
            Log("[menu] background: %s (%ux%u)", path, w, h);
        }
        return;
    }
}

#include "ui/m_menu_widgets.inc"

static void DrawBackdrop() {
    const float t=std::clamp(float(GetTickCount64()-g_openTick.load())/220.f,0.f,1.f);
    const auto close=g_closeTick.load();const float c=close?std::clamp(float(GetTickCount64()-close)/160.f,0.f,1.f):0;
    const float vis=(1-powf(1-t,3))*powf(1-c,3);
    auto*d=ImGui::GetBackgroundDrawList();const auto size=ImGui::GetIO().DisplaySize;const float i=MenuInset();
    const ImVec2 a{i,i+(1-vis)*Ui(10)},b{size.x-i,size.y-i+(1-vis)*Ui(10)};
    if(g_glass){
        // CSS's two unblurred offset shadows. Clip out the rounded panel interior
        // in a cached alpha texture, so shadow layers do not increase panel opacity.
        static ID3D11ShaderResourceView*srv=nullptr;static int sw=0,sh=0;
        const int w=int(size.x),h=int(size.y);
        if(g_device&&(w!=sw||h!=sh)){
            auto coverage=[](float x,float y,float left,float top,float right,float bottom,float r){
                const float qx=fabsf(x-(left+right)*.5f)-(right-left)*.5f+r,qy=fabsf(y-(top+bottom)*.5f)-(bottom-top)*.5f+r;
                const float dx=(std::max)(qx,0.f),dy=(std::max)(qy,0.f);const float distance=sqrtf(dx*dx+dy*dy)+(std::min)((std::max)(qx,qy),0.f)-r;
                return std::clamp(.5f-distance,0.f,1.f);
            };
            std::vector<unsigned>pixels(size_t(w)*h);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                const float panel=coverage(x+.5f,y+.5f,i,i,w-i,h-i,Ui(16));
                const float one=.16f*coverage(x+.5f,y+.5f,i,i+Ui(3),w-i,h-i+Ui(3),Ui(16));
                const float two=.28f*coverage(x+.5f,y+.5f,i,i+Ui(7),w-i+Ui(2),h-i+Ui(9),Ui(17));
                const unsigned alpha=unsigned(std::lround(255*(one+(1-one)*two)*(1-panel)));
                pixels[size_t(y)*w+x]=10u|(12u<<8)|(20u<<16)|(alpha<<24);
            }
            D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA data{pixels.data(),UINT(w*4),0};ID3D11Texture2D*texture=nullptr;ID3D11ShaderResourceView*next=nullptr;
            if(SUCCEEDED(g_device->CreateTexture2D(&desc,&data,&texture))){g_device->CreateShaderResourceView(texture,nullptr,&next);texture->Release();}
            if(next){if(srv)srv->Release();srv=next;sw=w;sh=h;}
        }
        if(srv)d->AddImage(ImTextureID(reinterpret_cast<uintptr_t>(srv)),{0,(1-vis)*Ui(10)},{size.x,size.y+(1-vis)*Ui(10)},{0,0},{1,1},ImGui::GetColorU32(Hex(0xFFFFFF,vis)));
    }
    d->AddRectFilled(a,b,ImGui::GetColorU32(Hex(0xFAFAF7,(g_glass?kMenuPanelAlpha:1.f)*vis)),Ui(16));
    d->AddRect(a,b,ImGui::GetColorU32(Hex(0x127A72,.18f*vis)),Ui(16),0,Ui(1));
    if(g_bgSrv&&g_bgShow){ImVec2 uv0{0,0},uv1{1,1};const float panelAspect=(b.x-a.x)/(b.y-a.y),imageAspect=float(g_bgW)/g_bgH;
        if(imageAspect>panelAspect){const float width=panelAspect/imageAspect;uv0.x=(1-width)*g_bgPosition;uv1.x=uv0.x+width;}else{const float height=imageAspect/panelAspect;uv0.y=(1-height)*g_bgPosition;uv1.y=uv0.y+height;}
        d->AddImageRounded(ImTextureID(reinterpret_cast<uintptr_t>(g_bgSrv)),a,b,uv0,uv1,ImGui::GetColorU32(Hex(0xFFFFFF,(1-g_bgDarkness/100.f)*.2f*vis)),Ui(16));}
}

// --- small building blocks ------------------------------------------------------------------



void PrettyBuildName(char* out, size_t n, const char* name) {
    static const char* const kPrefixes[] = { "PlayerDeco_", "Turret_Automated_", "BaseBuilding_Interactables_", "BaseBuilding_",
                                             "PU_Human_Enemy_GroundCombat_NPC_", "PU_Human-", "NPC_Archetypes-", "AIShip_CrewProfiles-" };
    if (const char* slash = strrchr(name, '/')) name = slash + 1;
    for (const char* p : kPrefixes)
        if (_strnicmp(name, p, strlen(p)) == 0) { name += strlen(p); break; }
    strncpy_s(out, n, name, _TRUNCATE);
    if (char* ext = strstr(out, ".socpak")) *ext = 0;
    if (char* guid = strstr(out, "_{")) {
        char tag[8];
        snprintf(tag, sizeof(tag), " (%.4s)", guid + 2);
        *guid = 0;
        strncat_s(out, n, tag, _TRUNCATE);
    }
    for (char* c = out; *c; ++c) if (*c == '_' || *c == '-') *c = ' ';
}

// =============================================================================================
// Menu settings
// =============================================================================================

static void DrawMenuTab() {
    static char search[96]="";SearchBox("##settingsSearch","搜索菜单设置",search,sizeof(search));
    auto show=[&](const char*t){return MatchesFilter(t,search);};
    SectionHeading("窗口");MListHeader();
    if(show("窗口位置 居中 重置"))if(MActionRow("reset","窗口位置","拖动标题栏移动","重置"))PostMessageW(g_wnd,WM_APP+74,0,0);
    if(show("固定尺寸 Windows DPI"))MActionRow("size","760 × 520 逻辑像素","跟随 Windows DPI 缩放",nullptr);
    SectionHeading("背景");
    if(g_bgSrv){if(show("显示背景图片"))V3Toggle("显示背景图片",&g_bgShow);ImGui::BeginDisabled(!g_bgShow);
        if(show("暗度"))MSliderInt("图片暗度##dark",&g_bgDarkness,20,95,"%d%%");
        if(show("图片位置")&&float(g_bgW)/g_bgH>.7f)MSliderFloat("图片位置##pos",&g_bgPosition,0.f,1.f,"%.2f");ImGui::EndDisabled();Hint(g_bgPath);
    }else Hint("把 menu_background.png 或 .jpg 放进 data 文件夹，重启游戏后生效。没放图片时这里不显示图片选项。");
    SectionHeading("关于");Hint(SCO_TITLE " 简体中文版 · " SCO_BASED_ON);Hint("上游：github.com/scubamount/sc-offline");
}

// =============================================================================================
// Window
// =============================================================================================

static void DrawStatusStrip(float height) {
    char status[256];Menu_GetStatus(status,sizeof(status));auto p=ImGui::GetCursorScreenPos();auto*d=ImGui::GetWindowDrawList();const float w=ImGui::GetContentRegionAvail().x;
    d->AddRectFilled(p,{p.x+w,p.y+height},ImGui::GetColorU32(Hex(0x10151D)),Ui(12),ImDrawFlags_RoundCornersBottom);
    d->AddCircleFilled({p.x+Ui(16),p.y+height*.5f},Ui(3.5f),ImGui::GetColorU32(kLeaf));
    ImGui::PushFont(nullptr,Ui(12)*kFontEmToPixel);MTextAt(d,{p.x+Ui(28),p.y+(height-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(Hex(0xD7EBE8)),status,w-Ui(280));
    d->AddText({p.x+w-Ui(240),p.y+(height-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(Hex(0x9A978E)),"M 菜单 · F4 视角 · F7 保存 · F8 返回");ImGui::PopFont();
    if(ImGui::IsMouseHoveringRect(p,{p.x+w,p.y+height}))ImGui::SetTooltip("%s",status);ImGui::Dummy({w,height});
}

// =============================================================================================
// The shell: sco.ui's tabs in order, then the Menu tab; overlays after the window
// =============================================================================================

static bool g_closeRequested = false;   // MenuClose(), during a frame
static bool g_backToFirstTab = false;   // MenuSelectFirstTab(), applied in the next frame

void MenuClose() { g_closeRequested = true; }
void MenuSelectFirstTab() { g_backToFirstTab = true; }

// One tab's page, drawn by its plugin as a callout under sco-core's crash guard. ImGui's error
// recovery puts its stacks back where they were before the call, so a draw that faulted half-way
// (sco-core then disables its plugin) or forgot an End() can't unbalance the menu's own.
static void DrawPluginTab(const sco::ui::TabInfo& tab, void* frame) {
    ImGuiErrorRecoveryState state;
    ImGui::ErrorRecoveryStoreState(&state);
    const sco::Result r = sco::ui::DrawTab(tab.id.c_str(), frame);
    ImGui::ErrorRecoveryTryToRecoverState(&state);
    if (r == sco::Result::Crashed) Log("[menu] tab %s (%s) crashed; plugin '%s' is disabled", tab.id.c_str(), tab.title.c_str(), tab.owner.c_str());
}

static void DrawPluginOverlays(void* frame) {
    ImGuiErrorRecoveryState state;
    ImGui::ErrorRecoveryStoreState(&state);
    sco::ui::DrawOverlays(frame);
    ImGui::ErrorRecoveryTryToRecoverState(&state);
}

static bool DrawMenu() {
    g_closeRequested=false;void*const frame=ImGui::GetCurrentContext();DrawBackdrop();
    const float enter=1-powf(1-std::clamp(float(GetTickCount64()-g_openTick.load())/220.f,0.f,1.f),3);
    const auto closing=g_closeTick.load();const float leave=closing?powf(1-std::clamp(float(GetTickCount64()-closing)/160.f,0.f,1.f),3):1.f;
    const float vis=enter*leave,inset=MenuInset();const auto display=ImGui::GetIO().DisplaySize;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,vis);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::SetNextWindowPos({inset,inset+(1-vis)*Ui(10)});ImGui::SetNextWindowSize({display.x-2*inset,display.y-2*inset});
    ImGui::Begin(SCO_TITLE "###main",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    auto p=ImGui::GetWindowPos();const float width=ImGui::GetWindowSize().x;auto*d=ImGui::GetWindowDrawList();
    ImGui::PushFont(g_menuBold,Ui(14)*kFontEmToPixel);d->AddText({p.x+Ui(16),p.y+Ui(12)},ImGui::GetColorU32(Hex(0x1F2A28)),"离线宇宙 · 菜单");ImGui::PopFont();
    const ImVec2 closePos{p.x+width-Ui(38),p.y+Ui(6)};ImGui::SetCursorScreenPos(closePos);
    if(ImGui::InvisibleButton("##close",{Ui(30),Ui(30)},ImGuiButtonFlags_EnableNav))g_closeRequested=true;
    const bool closeHover=ImGui::IsItemHovered();d->AddRectFilled(closePos,{closePos.x+Ui(30),closePos.y+Ui(30)},ImGui::GetColorU32(Hex(0xD4534A,closeHover?1.f:0.f)),Ui(9));
    const ImVec2 cc{closePos.x+Ui(15),closePos.y+Ui(15)};const auto closeColor=ImGui::GetColorU32(Hex(closeHover?0xFFFFFF:0x3A3934));
    d->AddLine({cc.x-Ui(4),cc.y-Ui(4)},{cc.x+Ui(4),cc.y+Ui(4)},closeColor,Ui(1.25f));d->AddLine({cc.x+Ui(4),cc.y-Ui(4)},{cc.x-Ui(4),cc.y+Ui(4)},closeColor,Ui(1.25f));
    d->AddLine({p.x,p.y+Ui(42)},{p.x+width,p.y+Ui(42)},ImGui::GetColorU32(Hex(0x127A72,.12f)));
    static std::string active,pending;static double leaveSince=0;const auto tabs=sco::ui::Tabs();bool found=active=="__menu";for(const auto&t:tabs)if(t.id==active)found=true;
    if(!pending.empty()&&ImGui::GetTime()-leaveSince>=.18){active=pending;pending.clear();g_v3PageSince=ImGui::GetTime();}
    if(!found||g_backToFirstTab){pending.clear();active=tabs.empty()?"__menu":tabs.front().id;g_v3PageSince=ImGui::GetTime();g_backToFirstTab=false;}
    ImGui::SetCursorScreenPos({p.x+Ui(10),p.y+Ui(50)});
    static float pillX=0,pillW=0,targetX=0,targetW=0;static bool pillReady=false;
    const float blend=1-expf(-18*ImGui::GetIO().DeltaTime);pillX+=(targetX-pillX)*blend;pillW+=(targetW-pillW)*blend;
    // Reserve an earlier draw channel for the moving capsule, beneath all tab text.
    d->ChannelsSplit(2);d->ChannelsSetCurrent(1);bool first=true;
    auto pill=[&](const char*title,const std::string&id){
        if(!first)ImGui::SameLine(0,Ui(2));first=false;const auto a=ImGui::GetCursorScreenPos();const float w=ImGui::CalcTextSize(title).x+Ui(20);const bool picked=(pending.empty()?active:pending)==id;
        if(picked){targetX=a.x-p.x;targetW=w;if(!pillReady){pillX=targetX;pillW=w;pillReady=true;}}
        ImGui::PushID(id.c_str());const bool hit=ImGui::InvisibleButton("tab",{w,Ui(28)});
        if(ImGui::IsItemHovered()&&!picked)d->AddRectFilled(a,{a.x+w,a.y+Ui(28)},ImGui::GetColorU32(Hex(0x127A72,.08f)),Ui(14));
        d->AddText({a.x+Ui(10),a.y+(Ui(28)-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(Hex(picked?0xFFFFFF:0x6F6D66)),title);
        if(hit&&(pending.empty()?active:pending)!=id){if(pending.empty())leaveSince=ImGui::GetTime();pending=id;targetX=a.x-p.x;targetW=w;}ImGui::PopID();
    };
    for(const auto&t:tabs)pill(t.title.c_str(),t.id);pill("菜单","__menu");
    d->ChannelsSetCurrent(0);d->AddRectFilled({p.x+pillX,p.y+Ui(50)},{p.x+pillX+pillW,p.y+Ui(78)},ImGui::GetColorU32(kLeaf),Ui(14));d->ChannelsMerge();
    d->AddLine({p.x,p.y+Ui(86)},{p.x+width,p.y+Ui(86)},ImGui::GetColorU32(Hex(0x127A72,.08f)));
    const float age=float(ImGui::GetTime()-g_v3PageSince),progress=std::clamp(age/.22f,0.f,1.f),ease=1-powf(1-progress,3);
    const float out= pending.empty()?0.f:std::clamp(float((ImGui::GetTime()-leaveSince)/.18),0.f,1.f);const float outEase=1-powf(1-out,3);
    ImGui::SetCursorScreenPos({p.x+Ui(16)+(pending.empty()?(1-ease)*Ui(8):-outEase*Ui(8)),p.y+Ui(98)});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,vis*(pending.empty()?ease:1-outEase));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    if(ImGui::BeginChild("##body",{width-Ui(32),ImGui::GetWindowHeight()-Ui(98+14+32)},ImGuiChildFlags_None)){
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,pending.empty()?ImGui::GetStyle().DisabledAlpha:1.f);ImGui::BeginDisabled(!pending.empty());
        if(active=="__menu")DrawMenuTab();else for(const auto&t:tabs)if(t.id==active){DrawPluginTab(t,frame);break;}
        ImGui::EndDisabled();ImGui::PopStyleVar();
    }ImGui::EndChild();ImGui::PopStyleVar(2);
    ImGui::SetCursorScreenPos({p.x,p.y+ImGui::GetWindowHeight()-Ui(32)});DrawStatusStrip(Ui(32));
    ImGui::End();ImGui::PopStyleVar(2);DrawPluginOverlays(frame);
    if(g_closeRequested&&!g_closeTick.load())g_closeTick=GetTickCount64();return !g_closeTick.load()||GetTickCount64()-g_closeTick.load()<160;
}

// =============================================================================================
// The frame, built on the game thread
// =============================================================================================
//
// sco.ui calls draw functions on the game thread only (a fault in one releases its plugin there),
// while this window and its D3D11 device belong to the menu thread. So the menu thread feeds ImGui
// (window messages, the Win32 and DX11 backends' NewFrame) and renders the draw data, and the game
// thread builds the frame in between: NewFrame, the shell with every tab, Render. They never use
// ImGui at the same time: the game thread only builds a frame it was asked for (Wanted ->
// Building), and the menu thread takes an unanswered request back before it touches ImGui.
//
// The menu thread never waits for the game: while a request is out it keeps pumping its window
// and polling M, and the window keeps its last picture until the next frame is built. It used to
// block on each frame, so a game thread that ran its message loop rarely (the game in the
// background behind the menu) made the menu crawl and M hard to get through.

enum : LONG { Frame_Idle, Frame_Wanted, Frame_Building, Frame_Built };
static volatile LONG g_frameState = Frame_Idle;
static HANDLE        g_frameBuilt = nullptr;   // auto-reset: wakes the menu thread when a frame is built
static bool          g_frameKeepOpen = true;   // DrawMenu's answer for the last frame
static std::atomic<bool> g_typing{ false };   // the menu has focus and a text box is active
static volatile LONG g_nudged = 0;            // a WM_NULL is on its way to the game
constexpr DWORD kMenuWaitMs = 50;

static bool GameThreadBuilding() { return g_frameState == Frame_Building; }

static LONG64 Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static double TicksToMs(LONG64 ticks) {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(freq.QuadPart);
}

void Menu_GameThreadFrame() {
    InterlockedExchange(&g_nudged, 0);
    if (InterlockedCompareExchange(&g_frameState, Frame_Building, Frame_Wanted) != Frame_Wanted) return;
    // The menu thread may call the Win32 backend's NewFrame more than once per built frame (each
    // time it takes a request back), so the time step comes from the frames built here.
    static LONG64 lastFrame = 0;
    const LONG64 start = Now();
    const float dt = lastFrame ? static_cast<float>(TicksToMs(start - lastFrame) / 1000.0) : 1.0f / 60.0f;
    lastFrame = start;
    ImGui::GetIO().DeltaTime = dt > 0.0001f ? (dt < 0.25f ? dt : 0.25f) : 0.0001f;
    ImGui::NewFrame();
    g_frameKeepOpen = DrawMenu();
    ImGui::Render();
    InterlockedExchange(&g_frameState, Frame_Built);
    SetEvent(g_frameBuilt);
}

bool Menu_Typing() { return g_typing.load(); }

// Menu thread: ask the game thread for a frame, and nudge its message loop once (the message hook
// runs on the game's next message).
static void RequestFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    InterlockedExchange(&g_frameState, Frame_Wanted);
    if (!InterlockedExchange(&g_nudged, 1)) PostMessageW(g_game, WM_NULL, 0, 0);
}

// Menu thread, while the game thread builds: answer only messages sent to the window (another
// thread waits on those; MenuWndProc keeps them from ImGui) until the frame is built.
static void WaitWhileBuilding() {
    while (g_frameState == Frame_Building) {
        MsgWaitForMultipleObjects(1, &g_frameBuilt, FALSE, kMenuWaitMs, QS_SENDMESSAGE);
        MSG msg;
        PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    }
}

static DWORD WINAPI MenuThread(LPVOID) {
    RefreshMenuSize();
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = MenuWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"starcitzenofflinemods_menu";
    RegisterClassExW(&wc);
    g_wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"sc-offline", WS_POPUP,
                            100, 100, g_menuW, g_menuH, nullptr, nullptr, wc.hInstance, nullptr);
    if (g_wnd) ApplyMenuCornerMask();
    ReadMenuPosition();
    g_frameBuilt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wnd || !g_frameBuilt || !CreateDevice()) return 0;
    RefreshMenuSize();SetWindowPos(g_wnd,nullptr,0,0,g_menuW,g_menuH,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);ApplyMenuCornerMask();

    IMGUI_CHECKVERSION();
    // The allocator plugins that draw with ImGui use too (menu_imgui.h).
    ImGui::SetAllocatorFunctions(sc_offline_imgui::Alloc, sc_offline_imgui::Free);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().MouseDrawCursor = false; // one OS cursor; no additional ImGui arrow
    // A plugin's draw that faults or leaves a Begin open is recovered from (DrawPluginTab), quietly.
    ImGui::GetIO().ConfigErrorRecoveryEnableAssert = false;
    ImGui::GetIO().ConfigErrorRecoveryEnableTooltip = false;
    ImGui::StyleColorsDark();
    ApplyTheme();
    LoadFonts();
    ImGui_ImplWin32_Init(g_wnd);
    ImGui_ImplDX11_Init(g_device, g_context);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadBackground();
    if (SUCCEEDED(com)) CoUninitialize();

    bool visible = false, wasDown = false;
    for (;;) {
        // Take ImGui back from a request the game thread hasn't started; let one it has finish.
        InterlockedCompareExchange(&g_frameState, Frame_Idle, Frame_Wanted);
        WaitWhileBuilding();
        // From here until RequestFrame the game thread doesn't touch ImGui.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // M is the product's own key (reserved in sco.ui): plugins can't bind it.
        const bool down = (GetAsyncKeyState('M') & 0x8000) != 0;
        const bool typing = visible && GetForegroundWindow() == g_wnd && ImGui::GetIO().WantTextInput;
        g_typing = typing;
        if (down && !wasDown && !typing && OurProcessHasFocus()) { if(visible){g_closeTick=g_closeTick.load()?0:GetTickCount64();}else{visible=true;ShowMenu(true);} }
        if(visible&&g_closeTick.load()&&GetTickCount64()-g_closeTick.load()>=160){visible=false;ShowMenu(false);}
        wasDown = down;
        if (visible && !IsWindowVisible(g_wnd)) { visible = false; ClipCursor(nullptr); g_clipped = false; }

        if (g_frameState == Frame_Built) {
            InterlockedExchange(&g_frameState, Frame_Idle);
            if (visible) {
                const float clear[4] = { g_glass?0.f:kSoil.x, g_glass?0.f:kSoil.y, g_glass?0.f:kSoil.z, g_glass?0.f:1.f };
                g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
                g_context->ClearRenderTargetView(g_rtv, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                g_swap->Present(1, 0);
                if (!g_frameKeepOpen) { visible = false; ShowMenu(false); }
            }
        }
        if (!visible) { g_typing = false; Sleep(50); continue; }
        KeepCursorInMenu();
        // Visible but not focused: keep the last picture and stop asking the game thread for
        // frames, so the game runs at full speed until the menu is focused again.
        if (GetForegroundWindow() != g_wnd) { Sleep(50); continue; }

        if (g_resizeW && g_resizeH) {
            ReleaseRenderTarget();
            g_swap->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            CreateRenderTarget();
        }

        RequestFrame();
        // Wakes for the built frame or the window's next message, whichever comes first.
        MsgWaitForMultipleObjects(1, &g_frameBuilt, FALSE, kMenuWaitMs, QS_ALLINPUT);
    }
}

void Menu_Start(HWND gameWindow) {
    g_game = gameWindow;
    if (HANDLE t = CreateThread(nullptr, 0, MenuThread, nullptr, 0, nullptr)) CloseHandle(t);
}
