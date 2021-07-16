#pragma once

class Camera
{
public:

    Camera();

    void              Update(float);
    void              MouseMoved(float, float);

    DirectX::XMMATRIX GetMatrix() const;

private:

    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT2 m_rotation;
};