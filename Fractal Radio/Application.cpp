#include "pch.h"

#include "Application.h"

using namespace std;

Application::Application(HINSTANCE hInstance, const wchar_t* applicationName, const uint32_t clientWidth,
                         const uint32_t clientHeight, uint8_t numFrames, bool useWarp, bool vSync)
{
    Window::CreateInstance(hInstance, applicationName, clientWidth, clientHeight);
    m_graphics = make_shared<Graphics>(useWarp, vSync, numFrames);
}

Application::~Application()
{
    Window::ResetInstance();
}