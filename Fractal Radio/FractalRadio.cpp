#include "pch.h"

#include "FractalRadio.h"

using namespace std;

FractalRadio::FractalRadio(const shared_ptr<Graphics> graphics) :
    Demo(graphics)
{
}

void FractalRadio::Update()
{
}

void FractalRadio::Render()
{
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

    const auto commandList = m_graphics->BeginFrame();
    m_graphics->ClearRenderTarget(commandList, clearColor);
    m_graphics->EndFrame(commandList);
}
