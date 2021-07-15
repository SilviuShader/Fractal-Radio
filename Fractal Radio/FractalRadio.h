#pragma once
#include "Demo.h"

class FractalRadio : public Demo
{
public:

    explicit FractalRadio(std::shared_ptr<Graphics>);

    void Update() override;
    void Render() override;
};
