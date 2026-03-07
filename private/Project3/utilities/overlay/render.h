#include <D3D11.h>
#include <D3DX11core.h>
#include <D3DX11.h>
#include <D3DX11tex.h>
#include <Windows.h>
#include <dwmapi.h>
#include <string>
#include <algorithm>
#include <cstring>
#pragma comment(lib, "dwmapi.lib")
#include "../str_obfuscate.hpp"
#include "../peb_modules.hpp"

static HINSTANCE _peb_hinstance() {
    return (HINSTANCE)peb_modules::get_image_base_address();
}

// Find Discord window - uses EnumWindows since title varies (Discord, Discord | #channel, Discord PTB, etc.)
static BOOL CALLBACK EnumDiscordWindowsProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd)) return TRUE;
    char title[512] = { 0 };
    GetWindowTextA(hwnd, title, sizeof(title));
    if (strlen(title) < 6) return TRUE; // Skip empty/short titles
    std::string t(title);
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    // Title contains "Discord" - handles "Discord", "Discord | General", "Discord PTB", "Discord Canary"
    if (t.find("discord") != std::string::npos) {
        *((HWND*)lParam) = hwnd;
        return FALSE; // Stop enumeration
    }
    return TRUE;
}

static HWND FindDiscordWindow()
{
    HWND result = nullptr;
    EnumWindows(EnumDiscordWindowsProc, (LPARAM)&result);
    return result;
}

#include "../utilities/sdk/cache/actorloop.h"
#include "../framework/imgui_impl_dx11.h"
#include "../str_obfuscate.hpp"
#include "../framework/imgui_impl_win32.h"
#include "../framework/imgui_internal.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "menu/drawing.h"
#include "menu/menu.h"
#include "other/fonts/font.h"
#include "../dependencies/loader/console.h"
#include "../framework/burbank.hpp"
#include "../framework/font_awesome.h"
#include "icons.h"
#include <iostream>
#include "../../fontsshit.h"
#include "../../logo.h"
#define ICE_IMPLEMENTATION
#include "../../ice.h"
void Error() 
{
    getchar();
    exit(-1);
}

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.f);
float menu_color[4] = { 1.00f, 0.00f, 0.20f, 0.80f };

int menu_tab = menus.tab;

ImFont* ico = nullptr;
ImFont* ico_combo = nullptr;
ImFont* ico_button = nullptr;
ImFont* ico_grande = nullptr;
ImFont* segu = nullptr;
ImFont* default_segu = nullptr;
ImFont* bold_segu = nullptr;

// Icon image textures for tabs (PNG images loaded from bytes in icons.h)
ID3D11ShaderResourceView* icon_aimbot_texture = nullptr;
ID3D11ShaderResourceView* icon_visual_texture = nullptr;
ID3D11ShaderResourceView* icon_misc_texture = nullptr;

// Picture ESP textures
ID3D11ShaderResourceView* ice_texture = nullptr;


HWND FortniteWindow = NULL;

ID3D11Device* d3d_device;
ID3D11DeviceContext* d3d_device_ctx;
//IDXGISwapChain* d3d_swap_chain;
ID3D11RenderTargetView* d3d_render_target;
D3DPRESENT_PARAMETERS d3d_present_params;
typedef struct _Header
{
    UINT Magic;
    UINT FrameCount;
    UINT NoClue;
    UINT Width;
    UINT Height;
    BYTE Buffer[1]; // B8G8R8A8
} Header;
#pragma comment(lib, "d3d11.lib")

namespace overlay
{
    bool InitImgui()
    {
        DXGI_SWAP_CHAIN_DESC swap_chain_description;
        ZeroMemory(&swap_chain_description, sizeof(swap_chain_description));
        swap_chain_description.BufferCount = 2;
        swap_chain_description.BufferDesc.Width = 0;
        swap_chain_description.BufferDesc.Height = 0;
        swap_chain_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_description.BufferDesc.RefreshRate.Numerator = 60;
        swap_chain_description.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_description.OutputWindow = globals.window_handle;
        swap_chain_description.SampleDesc.Count = 1;
        swap_chain_description.SampleDesc.Quality = 0;
        swap_chain_description.Windowed = 1;
        swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL d3d_feature_lvl;

        const D3D_FEATURE_LEVEL d3d_feature_array[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, d3d_feature_array, 2, D3D11_SDK_VERSION, &swap_chain_description, &d3d_swap_chain, &d3d_device, &d3d_feature_lvl, &d3d_device_ctx);
        if (FAILED(hr) || !d3d_swap_chain || !d3d_device || !d3d_device_ctx) {
            return false;
        }

        ID3D11Texture2D* pBackBuffer;
        D3DX11_IMAGE_LOAD_INFO info;
        D3DX11_IMAGE_LOAD_INFO image;
        ID3DX11ThreadPump* pump{ nullptr };

        d3d_swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

        d3d_device->CreateRenderTargetView(pBackBuffer, NULL, &d3d_render_target);

        pBackBuffer->Release();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig icons_config;

        ImFontConfig CustomFont;
        CustomFont.FontDataOwnedByAtlas = false;

        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.OversampleH = 2.5;
        icons_config.OversampleV = 2.5;

        ImGui_ImplWin32_Init(globals.window_handle);
        ImGui_ImplDX11_Init(d3d_device, d3d_device_ctx);

        ImFontConfig font_config;
        font_config.OversampleH = 1;
        font_config.OversampleV = 1;
        font_config.PixelSnapH = 1;

        static const ImWchar ranges[] =
        {
            0x0020, 0x00FF,
            0x0400, 0x044F,
            0,
        };

        // Load primary font
        fonts::medium = io.Fonts->AddFontFromMemoryTTF(Momo, sizeof(Momo), 15.0f, &font_config, ranges);
        if (!fonts::medium) {
            fonts::medium = io.Fonts->AddFontFromMemoryTTF(InterMedium, sizeof(InterMedium), 15.0f, &font_config, ranges);
        }
        if (fonts::medium) {
            io.FontDefault = fonts::medium;
        }

        menus.MenuFont = io.Fonts->AddFontFromMemoryTTF(cBurbank, sizeof(cBurbank), 16.f); /*io.Fonts->AddFontFromFileTTF(("C:\\Windows\\Fonts\\calibrib.ttf"), 18.f);*/
        if (zLogo == nullptr) D3DX11CreateShaderResourceViewFromMemory(d3d_device, Data, sizeof(Data), &info, pump, &zLogo, 0);


        
        // Load additional fonts
        mainfont = io.Fonts->AddFontFromMemoryTTF(&mainfonthxd, sizeof mainfonthxd, 16, NULL, io.Fonts->GetGlyphRangesCyrillic());
        fonts::semibold = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 17.0f, &font_config, ranges);
        fonts::logo = io.Fonts->AddFontFromMemoryTTF(catrine_logo, sizeof(catrine_logo), 17.0f, &font_config, ranges);
        
        // Load icon textures
        D3DX11_IMAGE_LOAD_INFO icon_image_info;
        ZeroMemory(&icon_image_info, sizeof(D3DX11_IMAGE_LOAD_INFO));
        icon_image_info.Width = D3DX11_DEFAULT;
        icon_image_info.Height = D3DX11_DEFAULT;
        icon_image_info.Depth = D3DX11_DEFAULT;
        icon_image_info.FirstMipLevel = D3DX11_DEFAULT;
        icon_image_info.MipLevels = 1;
        icon_image_info.Usage = D3D11_USAGE_DEFAULT;
        icon_image_info.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        icon_image_info.CpuAccessFlags = 0;
        icon_image_info.MiscFlags = 0;
        icon_image_info.Format = DXGI_FORMAT_FROM_FILE;
        icon_image_info.Filter = D3DX11_FILTER_NONE;
        icon_image_info.MipFilter = D3DX11_FILTER_NONE;
        icon_image_info.pSrcInfo = NULL;
        
        if (icon_aimbot_size > 1) {
            D3DX11CreateShaderResourceViewFromMemory(d3d_device, icon_aimbot_bytes, icon_aimbot_size, &icon_image_info, pump, &icon_aimbot_texture, 0);
        }
        if (icon_visual_size > 1) {
            D3DX11CreateShaderResourceViewFromMemory(d3d_device, icon_visual_bytes, icon_visual_size, &icon_image_info, pump, &icon_visual_texture, 0);
        }
        if (icon_misc_size > 1) {
            D3DX11CreateShaderResourceViewFromMemory(d3d_device, icon_misc_bytes, icon_misc_size, &icon_image_info, pump, &icon_misc_texture, 0);
        }
        
        // Load Charlie ESP texture
        if (charlie_png_len > 1) {
            D3DX11CreateShaderResourceViewFromMemory(d3d_device, charlie_png, charlie_png_len, &icon_image_info, pump, (ID3D11ShaderResourceView**)&visuals.picture_texture, 0);
        }
        
       


        return true;
    }
    
    #define WM_REQUEST_FG (WM_APP + 0x100)
    static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_REQUEST_FG) {
            if (GetForegroundWindow() != hwnd) SetForegroundWindow(hwnd);
            return 0;
        }
        LRESULT ir = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        if (ir) return ir;  // ImGui handled it
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    bool renderwin()
    {
        // Create render window (own overlay path)
        WNDCLASSEXA wcex = {
                sizeof(WNDCLASSEXA),
                CS_HREDRAW | CS_VREDRAW,
                HostWndProc,
                0,
                0,
                _peb_hinstance(),
                LoadIcon(nullptr, IDI_APPLICATION),
                LoadCursor(nullptr, IDC_ARROW),
                (HBRUSH)CreateSolidBrush(RGB(0, 0, 0)),
                nullptr,
                nullptr,
                LoadIcon(nullptr, IDI_APPLICATION)
        };
        auto ovCls = OBF_STR("Win32HostWindow");
        auto ovTitle = OBF_STR("Window");
        wcex.lpszClassName = ovCls.c_str();

        RegisterClassExA(&wcex);
        globals.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        globals.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        globals.window_handle = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
            ovCls.c_str(),
            ovTitle.c_str(),
            WS_POPUP,
            0, 0,
            globals.ScreenWidth,
            globals.ScreenHeight,
            NULL,
            NULL,
            _peb_hinstance(),
            NULL
        );

        if (!globals.window_handle) {
            return false;
        }

        // Make window transparent and layered
        SetLayeredWindowAttributes(globals.window_handle, RGB(0, 0, 0), 255, LWA_ALPHA);
        
        // Extend frame into client area for transparency
        MARGINS margin = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(globals.window_handle, &margin);

        // Set window to topmost
        SetWindowPos(globals.window_handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // Show window
        ShowWindow(globals.window_handle, SW_SHOW);
        UpdateWindow(globals.window_handle);

        return true;
    }
    

    
    bool HijackWindow()
    {
        // Get screen dimensions first
        globals.ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        globals.ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // Try Discord first if enabled, otherwise fall back to CrosshairX
        if (globals.use_discord) {
            globals.window_handle = FindDiscordWindow();
        } else {
            // CrosshairX targeting
            globals.window_handle = FindWindowA(OBF_STR("Chrome_WidgetWin_1").c_str(), OBF_STR("CrosshairX").c_str()); 
        }

        if (!globals.window_handle)
        {
            // This should not happen if console check passed, but handle it anyway
            return false;
        }

        SetWindowPos(globals.window_handle, HWND_TOPMOST, 0, 0, globals.ScreenWidth, globals.ScreenHeight, SWP_SHOWWINDOW);
        SetLayeredWindowAttributes(globals.window_handle, RGB(0, 0, 0), 255, LWA_ALPHA);
       SetWindowLongA(globals.window_handle, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT);

        MARGINS margin = { -1 };
        DwmExtendFrameIntoClientArea(globals.window_handle, &margin);

        ShowWindow(globals.window_handle, SW_SHOW);
        UpdateWindow(globals.window_handle);

        ShowWindow(globals.window_handle, SW_HIDE);
        
        return true;
    }
    
    void menu_loop()
    {
        // Don't let menu-key toggle close the menu when user is clicking (e.g. on tabs)
        static DWORD last_toggle_time = 0;
        bool menu_key_pressed = (GetAsyncKeyState(menus.menu_key) & 1) != 0;
        if (menu_key_pressed) {
            bool is_mouse_key = (menus.menu_key == VK_LBUTTON || menus.menu_key == VK_RBUTTON ||
                menus.menu_key == VK_MBUTTON || menus.menu_key == VK_XBUTTON1 || menus.menu_key == VK_XBUTTON2);
            bool imgui_capturing = ImGui::GetIO().WantCaptureMouse;
            DWORD now = GetTickCount();
            bool debounce_ok = (now - last_toggle_time) > 150;
            if (debounce_ok && !(is_mouse_key && menus.ShowMenu) && !(imgui_capturing && menus.ShowMenu)) {
                menus.ShowMenu = !menus.ShowMenu;
                last_toggle_time = now;
            }
        }

        if (GetAsyncKeyState(VK_END) & 1)
        {
            console.kill_cheat();
        }

        // Make window click-through when menu is closed, interactive when open
        if (globals.window_handle) {
            static bool was_menu_open = false;
            LONG_PTR exStyle = GetWindowLongPtr(globals.window_handle, GWL_EXSTYLE);
            const bool want_transparent = !menus.ShowMenu;
            const bool is_transparent = (exStyle & WS_EX_TRANSPARENT) != 0;
            // Only modify exStyle when it actually needs to change (avoids redundant syscalls and potential flicker)
            if (want_transparent != is_transparent) {
                SetWindowLongPtr(globals.window_handle, GWL_EXSTYLE,
                    want_transparent ? (exStyle | WS_EX_TRANSPARENT) : (exStyle & ~WS_EX_TRANSPARENT));
            }
            if (menus.ShowMenu) {
                if (!was_menu_open) {
                    // Defer SetForegroundWindow to avoid blocking the frame (can cause ~2s hang when stealing focus)
                    PostMessage(globals.window_handle, WM_REQUEST_FG, 0, 0);
                    was_menu_open = true;
                }
            } else {
                was_menu_open = false;
            }
        }

        if (menus.ShowMenu)
        {
            menu_class();
        }
    }

    void draw()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        actorloop();

        menu_loop();

        if (menus.ShowMenu)
        {
            if (menus.menu_cursor)
            {
              
                ImGuiIO& io = ImGui::GetIO();
                ImVec2 cursor_pos = ImGui::GetMousePos();
                float size = 15.0f;  // Size of the triangle
                ImDrawList* draw_list = ImGui::GetForegroundDrawList();
                ImU32 color = IM_COL32(255, 0, 0, 255);

                // Define the three vertices of the triangle to point like a regular cursor
                ImVec2 p1 = cursor_pos;  // Tip of the triangle (pointing)
                ImVec2 p2 = ImVec2(cursor_pos.x - size * 0.5f, cursor_pos.y + size);  // Bottom-left of the triangle
                ImVec2 p3 = ImVec2(cursor_pos.x + size * 0.5f, cursor_pos.y + size);  // Bottom-right of the triangle

                // Draw the filled triangle
                draw_list->AddTriangleFilled(p1, p2, p3, color);

            }
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        d3d_device_ctx->OMSetRenderTargets(1, &d3d_render_target, nullptr);
        d3d_device_ctx->ClearRenderTargetView(d3d_render_target, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Handle presentation in actorloop only to avoid double presentation
        // Presentation logic moved to actorloop.h to prevent FPS issues
    }

    bool render()
    {
        MSG msg = { NULL };
        ZeroMemory(&msg, sizeof(MSG));
        UpdateWindow(globals.window_handle);
        ShowWindow(globals.window_handle, SW_SHOW);
        // Own overlay: brief settle for D3D/window (reduced from 300ms for faster startup)
        if (!globals.use_crosshairx && !globals.use_discord) {
            Sleep(80);
        }
        // Render thread above normal - ensures menu/ESP get driver over CacheLevels (fixes 1-6 FPS monopoly)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        while (msg.message != WM_QUIT)
        {
            // Process ALL pending messages so ImGui gets input reliably (fixes tab clicks)
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) break;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            ImGuiIO& io = ImGui::GetIO();
            // Frame timing - ALWAYS set DeltaTime so FPS display is correct (fixes stuck 1-6 FPS)
            LARGE_INTEGER frame_begin = {};
            {
                static LARGE_INTEGER frequency, last_time;
                static bool timer_init = false;
                if (!timer_init) { QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&last_time); timer_init = true; }
                QueryPerformanceCounter(&frame_begin);
                float delta_time = (float)(frame_begin.QuadPart - last_time.QuadPart) / (float)frequency.QuadPart;
                last_time = frame_begin;
                io.DeltaTime = (delta_time > 0.0001f && delta_time < 0.5f) ? delta_time : 1.0f / 60.0f;
            }

            // When menu is OPEN: let Win32 backend handle all input (WM_MOUSEMOVE, WM_LBUTTONDOWN, etc.)
            // When menu is CLOSED: overlay is click-through, so manually feed cursor pos and button for aimbot/ESP
            if (!menus.ShowMenu) {
                static POINT last_cursor = {-1, -1};
                POINT current_cursor;
                GetCursorPos(&current_cursor);
                if (current_cursor.x != last_cursor.x || current_cursor.y != last_cursor.y) {
                    io.MousePos.x = (float)current_cursor.x;
                    io.MousePos.y = (float)current_cursor.y;
                    last_cursor = current_cursor;
                }
                io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            }

            draw();

            // Own overlay: frame pacing - only sleep REMAINING time in frame budget (fixes 1-6 FPS when draw is slow)
            if (!globals.use_crosshairx && !globals.use_discord) {
                if (visuals.vsync) {
                    Sleep(0);  // Yield only - Present(1) already blocks until vsync
                } else {
                    static LARGE_INTEGER freq_pace;
                    static bool freq_init = false;
                    if (!freq_init) { QueryPerformanceFrequency(&freq_pace); freq_init = true; }
                    LARGE_INTEGER frame_end;
                    QueryPerformanceCounter(&frame_end);
                    int fps = (visuals.overlay_fps >= 30 && visuals.overlay_fps <= 240) ? visuals.overlay_fps : 60;
                    float frame_budget_ms = 1000.0f / (float)fps;
                    float elapsed_ms = (float)(frame_end.QuadPart - frame_begin.QuadPart) * 1000.0f / (float)freq_pace.QuadPart;
                    DWORD sleep_ms = (DWORD)(frame_budget_ms - elapsed_ms);
                    if (sleep_ms > 0 && sleep_ms < 500) Sleep(sleep_ms);  // Only cap when ahead of schedule
                }
            } else {
                Sleep(2 + (GetTickCount64() & 1));
            }
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        DestroyWindow(globals.window_handle);

        return true;
    }

    void start()
    {
        // Choose overlay method based on settings (set in console at startup)
        bool success = false;
        if (globals.use_crosshairx || globals.use_discord) {
            // Use window hijacking (lower CPU usage)
            success = HijackWindow();
            if (!success) {
                // Fall back to own overlay - never close FrozenPublic
                globals.use_discord = false;
                globals.use_crosshairx = false;
                success = renderwin();
            }
        }
        if (!success) {
            success = renderwin();
        }
        if (!success) {
            return;
        }
        
        if (!InitImgui()) {
            globals.window_handle = nullptr;
            globals.use_discord = false;
            globals.use_crosshairx = false;
            if (!renderwin()) {
                return;
            }
            if (!InitImgui()) {
                return;
            }
        }

        // Load saved config if exists so settings persist
        load_config("hezux_config.ini");

        // Do NOT run get_camera at startup - it does driver reads and can block/freeze if game is loading.
        // get_camera is called from actorloop when needed (aimbot target).
        render();
    }
}