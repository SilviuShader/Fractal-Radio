#include "pch.h"

#include "Camera.h"

using namespace DirectX;

constexpr auto MOVE_SPEED  = 10.0f;
constexpr auto MOUSE_SPEED = 0.0005f;

Camera::Camera() :
    m_position(XMFLOAT3(0.0f, 0.0f, -5.0f)),
    m_rotation(0.0f, 0.0f)
{
}

void Camera::Update(float deltaTime)
{
    auto translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMMATRIX rotationMatrix = XMMatrixRotationX(m_rotation.x);
    rotationMatrix = XMMatrixMultiply(rotationMatrix, XMMatrixRotationY(m_rotation.y));

    if (GetKeyState('W') & 0x8000)
    {
        auto dir = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
        auto loadedVec = XMLoadFloat4(&dir);
        auto newVec = XMVector4Transform(loadedVec, rotationMatrix);
        XMStoreFloat4(&dir, newVec);

        translation = XMFLOAT3(translation.x + dir.x, translation.y + dir.y, translation.z + dir.z);
    }

    if (GetKeyState('A') & 0x8000)
    {
        auto dir = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
        auto loadedVec = XMLoadFloat4(&dir);
        auto newVec = XMVector4Transform(loadedVec, rotationMatrix);
        XMStoreFloat4(&dir, newVec);

        translation = XMFLOAT3(translation.x + dir.x, translation.y + dir.y, translation.z + dir.z);
    }

    if (GetKeyState('S') & 0x8000)
    {
        auto dir = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
        auto loadedVec = XMLoadFloat4(&dir);
        auto newVec = XMVector4Transform(loadedVec, rotationMatrix);
        XMStoreFloat4(&dir, newVec);

        translation = XMFLOAT3(translation.x + dir.x, translation.y + dir.y, translation.z + dir.z);
    }

    if (GetKeyState('D') & 0x8000)
    {
        auto dir = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
        auto loadedVec = XMLoadFloat4(&dir);
        auto newVec = XMVector4Transform(loadedVec, rotationMatrix);
        XMStoreFloat4(&dir, newVec);

        translation = XMFLOAT3(translation.x + dir.x, translation.y + dir.y, translation.z + dir.z);
    }

    auto loadedVec = XMLoadFloat3(&translation);
    auto normalizedVec = XMVector3Normalize(loadedVec);
    auto speedVec = normalizedVec * MOVE_SPEED * deltaTime;
    XMStoreFloat3(&translation, speedVec);

    m_position = XMFLOAT3(m_position.x + translation.x, m_position.y + translation.y, m_position.z + translation.z);
}

void Camera::MouseMoved(float diffX, float diffY)
{
    m_rotation.x += diffY * MOUSE_SPEED;
    m_rotation.y += diffX * MOUSE_SPEED;

    if (m_rotation.x >= XM_PIDIV2)
        m_rotation.x = XM_PIDIV2;
    if (m_rotation.x <= -XM_PIDIV2)
        m_rotation.x = XM_PIDIV2;

    if (m_rotation.y < 0.0f)
        m_rotation.y += XM_2PI;
    else
        if (m_rotation.y >= XM_2PI)
            m_rotation.y -= XM_2PI;
}

XMMATRIX Camera::GetMatrix() const
{
    XMMATRIX result = XMMatrixRotationX(m_rotation.x);
    result = XMMatrixMultiply(result, XMMatrixRotationY(m_rotation.y));
    result = XMMatrixMultiply(result, XMMatrixTranslation(m_position.x, m_position.y, m_position.z));
    return result;
}