#pragma once

#include "Window.h"
#include "Graphics.h"

#include <memory>

class Application
{
public:

    Application(HINSTANCE, const wchar_t*, uint32_t, uint32_t, uint8_t, bool, bool);
    ~Application();

    template <class T>
    void Run() const;

private:

    std::shared_ptr<Graphics> m_graphics{};
};

template <class T>
void Application::Run() const
{
    std::shared_ptr<T> demo = std::make_shared<T>(m_graphics);

    Window::GetInstance()->Run(demo);
}
