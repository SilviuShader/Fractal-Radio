#include "pch.h"
#include "Window.h"

#pragma warning (disable : 6387)

using namespace std;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

constexpr auto WINDOW_CLASS_NAME = L"Window Class";

Window* Window::g_instance = nullptr;

// ReSharper disable once CppParameterMayBeConst
Window::Window(HINSTANCE hInstance, const wchar_t* applicationName, const uint32_t clientWidth, const uint32_t clientHeight) :
    m_fullscreen(false),
    m_windowRect{},
    m_clientWidth(clientWidth),
    m_clientHeight(clientHeight)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterWindowClass(hInstance, WINDOW_CLASS_NAME);
    m_hWnd = CreateWindow(WINDOW_CLASS_NAME, hInstance, applicationName, m_clientWidth, m_clientHeight);
    ShowWindow(m_hWnd, SW_SHOW);
}


// Based on https://www.3dgep.com/learning-directx-12-1/#the-main-entry-point
void Window::Run(const shared_ptr<Demo> demo)
{
    m_demo = demo;
    
    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    m_demo.reset();
}

void Window::CreateInstance(HINSTANCE hInstance, const wchar_t* applicationName, const uint32_t clientWidth,
                            const uint32_t clientHeight)
{
    if (g_instance)
        ResetInstance();

    g_instance = new Window(hInstance, applicationName, clientWidth, clientHeight);
}

Window* Window::GetInstance()
{
    return g_instance;
}

void Window::ResetInstance()
{
    if (g_instance)
    {
        delete g_instance;
        g_instance = nullptr;
    }
}


// Based on https://www.3dgep.com/learning-directx-12-1/#fullscreen-state
void Window::SetFullscreen(const bool fullscreen)
{
    if (m_fullscreen != fullscreen)
    {
        m_fullscreen = fullscreen;

        if (m_fullscreen)
        {
            GetWindowRect(m_hWnd, &m_windowRect);
            const UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

            SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);

            // ReSharper disable once CppLocalVariableMayBeConst
            HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(hMonitor, &monitorInfo);

            SetWindowPos(m_hWnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
        else
        {
            SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            SetWindowPos(m_hWnd, HWND_NOTOPMOST,
                m_windowRect.left,
                m_windowRect.top,
                m_windowRect.right - m_windowRect.left,
                m_windowRect.bottom - m_windowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_hWnd, SW_NORMAL);
        }
    }
}


RECT Window::GetRect() const
{
    RECT result;
    GetWindowRect(m_hWnd, &result);
    return result;
}

HWND Window::GetHWnd() const
{
    return m_hWnd;
}

uint32_t Window::GetClientWidth() const
{
    return m_clientWidth;
}

uint32_t Window::GetClientHeight() const
{
    return m_clientHeight;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#create-window-instance
// ReSharper disable once CppParameterMayBeConst
HWND Window::CreateWindow(const wchar_t* windowClassName, HINSTANCE hInstance, const wchar_t* windowTitle,
                          const uint32_t width, const uint32_t height) const
{
    const auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    const auto windowWidth = windowRect.right - windowRect.left;
    const auto windowHeight = windowRect.bottom - windowRect.top;

    const auto windowX = max<int>(0, (screenWidth - windowWidth) / 2);
    const auto windowY = max<int>(0, (screenHeight - windowHeight) / 2);

    const auto hWnd = CreateWindowExW(
        NULL,
        windowClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        MessageBox(nullptr, L"Failed to create window.", L"Error", MB_OK);
        assert(false);
    }

    return hWnd;
}

// Based on https://www.3dgep.com/learning-directx-12-1/#register-the-window-class
// ReSharper disable once CppParameterMayBeConst
void Window::RegisterWindowClass(HINSTANCE hInstance, const wchar_t* windowClassName)
{
    WNDCLASSEXW windowClass;

    windowClass.cbSize = sizeof WNDCLASSEX;
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &WndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = LoadIcon(hInstance, nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = LoadIcon(hInstance, nullptr);

    static auto atom = RegisterClassExW(&windowClass);

    if (atom <= 0)
    {
        MessageBox(nullptr, L"Could not register window class.", L"Error", MB_OK);
        assert(false);
    }
}

// ReSharper disable once CppParameterMayBeConst
LRESULT CALLBACK WndProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    Window* instance = Window::GetInstance();

    if (instance && instance->m_demo && instance->m_demo->GetGraphics()->IsInitialized())
    {
        switch (message)
        {
        case WM_PAINT:
            instance->m_demo->Update();
            instance->m_demo->Render();
            break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            {
                const auto isAltPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

                switch (wParam)
                {
                case 'V':
                    instance->m_demo->GetGraphics()->ToggleVSync();
                    break;
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
                case VK_RETURN:
                    if (isAltPressed)
                    {
                case VK_F11:
                    instance->SetFullscreen(!instance->m_fullscreen);
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        case WM_SYSCHAR:
            break;
        case WM_SIZE:
            {
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);

                const UINT width = clientRect.right - clientRect.left;
                const UINT height = clientRect.bottom - clientRect.top;

                if (instance->m_clientWidth != width || instance->m_clientHeight != height)
                {
                    instance->m_clientWidth = max(1u, width);
                    instance->m_clientHeight = max(1u, height);
                    instance->m_demo->GetGraphics()->Resize(width, height);
                    instance->m_demo->Resize(width, height);
                }
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    else
        return DefWindowProc(hWnd, message, wParam, lParam);

    return 0;
}
