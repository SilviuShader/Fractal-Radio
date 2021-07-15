#pragma once

#include <functional>

#include "Demo.h"

#if defined(CreateWindow)
#undef CreateWindow
#endif

class Demo;

class Window
{
    Window(HINSTANCE, const wchar_t*, uint32_t, uint32_t);

public:

           void     Run(std::shared_ptr<Demo>);

           void     SetFullscreen(bool);

           RECT     GetRect()         const;
           HWND     GetHWnd()         const;

           uint32_t GetClientWidth()  const;
           uint32_t GetClientHeight() const;
    
    static void     CreateInstance(HINSTANCE, const wchar_t*, uint32_t, uint32_t);
    static Window*  GetInstance();
    static void     ResetInstance();

private:

           HWND                             CreateWindow(const wchar_t*, HINSTANCE, const wchar_t*, uint32_t, uint32_t) const;
    static void                             RegisterWindowClass(HINSTANCE, const wchar_t*);
    
    friend LRESULT CALLBACK                 WndProc(HWND, UINT, WPARAM, LPARAM);


    HWND                                    m_hWnd;
    bool                                    m_fullscreen;
    RECT                                    m_windowRect;
    std::shared_ptr<Demo>                   m_demo;

    uint32_t                                m_clientWidth;
    uint32_t                                m_clientHeight;

    // ReSharper disable once CppInconsistentNaming
    static Window*                          g_instance;
};
