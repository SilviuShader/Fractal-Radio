#pragma once

#include "Graphics.h"

class Graphics;

class Demo  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:

    explicit Demo(std::shared_ptr<Graphics>);
    virtual  ~Demo();

            std::shared_ptr<Graphics> GetGraphics() const;

    virtual void                      Update(float) = 0;
    virtual void                      Render()      = 0;

    virtual void                      Resize(uint32_t, uint32_t);
    virtual void                      MouseMoved(float, float);

protected:

    std::shared_ptr<Graphics> m_graphics; 
};
