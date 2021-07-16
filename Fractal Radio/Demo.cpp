#include "pch.h"

#include "Demo.h"

using namespace std;

Demo::Demo(const shared_ptr<Graphics> graphics) :
    m_graphics(graphics)
{
}

Demo::~Demo()
{
}

shared_ptr<Graphics> Demo::GetGraphics() const
{
    return m_graphics;
}

void Demo::Resize(uint32_t, uint32_t)
{
}

void Demo::MouseMoved(float, float)
{
}
