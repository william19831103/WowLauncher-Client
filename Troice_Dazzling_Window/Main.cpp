#pragma execution_character_set("utf-8")

// Windows 相关头文件应该在最前面
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// 其他头文件
#include "main.h"
#include "GameManager.h"

// 定义全局变量
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
ID3D11ShaderResourceView* g_background = nullptr;  // 定义 g_background

// 函数声明
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd
)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Troice Dazzling Window", WS_POPUP, 100, 100, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   
    
    ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\MSYH.TTC", 20.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 添加帧率控量
    const float TARGET_FPS = 60.0f;
    const float TARGET_FRAMETIME = 1000.0f / TARGET_FPS;
    LARGE_INTEGER frequency, last_time;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last_time);

    // Main loop
    bool done = false;
    while (!done)
    {
        // 计算当前帧时间
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        float delta_time = (float)(current_time.QuadPart - last_time.QuadPart) * 1000.0f / frequency.QuadPart;

        // 如果距离上一帧时间太短，则待
        if (delta_time < TARGET_FRAMETIME)
        {
            Sleep((DWORD)(TARGET_FRAMETIME - delta_time));
            continue;
        }

        last_time = current_time;

        // 处理消息
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Draw Window
        {
            MainWindow();
        }


        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 修改 UTF-8 转换函数
std::string utf8_to_gbk(const char* utf8_str) {
    try {
        // 先将 UTF-8 转换为 Unicode
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
        if (wlen == 0) return "";
        
        std::vector<wchar_t> wstr(wlen);
        if (MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr.data(), wlen) == 0) {
            return "";
        }

        // 再将 Unicode 转换为 GBK
        int len = WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
        if (len == 0) return "";
        
        std::vector<char> str(len);
        if (WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, str.data(), len, NULL, NULL) == 0) {
            return "";
        }

        return std::string(str.data());
    }
    catch (...) {
        return "";
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {

            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;

    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}



void MainWindow() {
    static bool open = true;
    static bool first_time = true;
    static int bg_width = 0, bg_height = 0;
    static bool serverOnline = true;
    static HWND main_hwnd = NULL;
    
    if (first_time)
    {
        LoadTextureFromFile("Queen.jpg", &g_background, &bg_width, &bg_height);
        main_hwnd = GetActiveWindow();
        
        // 直接调用 initialize_server_info
        initialize_server_info(main_hwnd);
        
        first_time = false;
    }

    if (open) {
        // 设置窗口和按钮的样式
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 12.0f;     // 窗口圆角
        style.FrameRounding = 12.0f;      // 按钮圆角
        style.PopupRounding = 12.0f;      // 弹出窗口圆角
        style.ScrollbarRounding = 12.0f;  // 滚动条圆角
        style.GrabRounding = 12.0f;       // 滑块圆角
        style.TabRounding = 12.0f;        // 标签页圆角

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1000, 600));
        
        ImGui::Begin("魔兽世界登录器", &open, 
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoCollapse);

        // 绘制背景
        ImGui::GetWindowDrawList()->AddImage(
            (void*)g_background,
            ImGui::GetWindowPos(),
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x,
                   ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)
        );

        // 设置按钮样式 - 调亮蓝色背景
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.5f, 1.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.6f, 1.0f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        // 添加关闭按钮
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 40, 10));
        if (ImGui::Button("X", ImVec2(30, 30))) {
            open = false;
        }

        // 计算底部按钮的位置和间距
        float button_width = 150;
        float button_height = 40;
        float window_width = ImGui::GetWindowSize().x;
        float total_buttons_width = button_width * 4;
        float spacing = (window_width - total_buttons_width - 100) / 3;
        float start_x = 50;
        float start_y = ImGui::GetWindowSize().y - button_height - 50;

        // 获取当前字体大小
        float originalFontSize = ImGui::GetFontSize() * 1.3f;  // 因为文字缩放是1.3倍
        
        // 服务器名称和状态指示器
        ImGui::SetCursorPos(ImVec2(start_x, 20));
        
        // 绘制状态指示器圆圈
        float circle_radius = 8.0f;
        ImVec2 circle_pos = ImGui::GetCursorScreenPos();
        ImVec4 circle_color = serverOnline ? 
            ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :  // 在线时为绿色
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f);   // 离线时为灰色
        
        // 调整圆圈的垂位置，使其与文字中心对齐
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(circle_pos.x + circle_radius, 
                  circle_pos.y + (originalFontSize/2) + 2),  // +2 用于微调
            circle_radius,
            ImGui::ColorConvertFloat4ToU32(circle_color)
        );

        // 移动文本位置，为圆圈留出空间
        ImGui::SetCursorPos(ImVec2(start_x + circle_radius*2 + 10, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.3f);
        
        if (!ServerInfo::name.empty() && ServerInfo::isConnected) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, ServerInfo::name.c_str(), -1, NULL, 0);
            if (wlen > 0) {
                std::vector<wchar_t> wstr(wlen);
                if (MultiByteToWideChar(CP_UTF8, 0, ServerInfo::name.c_str(), -1, wstr.data(), wlen) > 0) {
                    ImGui::Text("%s", ServerInfo::name.c_str());
                }
            }
        } else {
            ImGui::Text("炽焰战网");
        }
        
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();

        // 更新服务器状态指示器
        serverOnline = ServerInfo::isConnected;

        // 通知区域
        ImGui::SetCursorPos(ImVec2(start_x + button_width + spacing, 20));
        ImGui::BeginChild("通知区域", ImVec2(600, 400), true);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(ServerInfo::notice.c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();

        // 底部按钮 - 所有四个按钮
        ImGui::SetCursorPos(ImVec2(start_x, start_y));
        if (ImGui::Button("注册账号", ImVec2(button_width, button_height))) {
            // 处理注册账号按钮点击
        }

        ImGui::SetCursorPos(ImVec2(start_x + button_width + spacing, start_y));
        if (ImGui::Button("赞助服务", ImVec2(button_width, button_height))) {
            // 处理赞助服务按钮点击
        }

        ImGui::SetCursorPos(ImVec2(start_x + (button_width + spacing) * 2, start_y));
        if (ImGui::Button("进入QQ群", ImVec2(button_width, button_height))) {
            // 处理进入QQ群按钮点击
        }

        ImGui::SetCursorPos(ImVec2(start_x + (button_width + spacing) * 3, start_y));
        if (ImGui::Button("启动游戏", ImVec2(button_width, button_height))) {
            check_and_start_game(GetActiveWindow());  // 使用GameManager中定义的函数
        }

        // 恢复按钮样式
        ImGui::PopStyleColor(4);

        ImGui::End();
    }
    else {
        exit(0);
    }
}



















