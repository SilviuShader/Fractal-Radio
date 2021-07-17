#include "pch.h"

#include "Camera.h"

#include "Window.h"

using namespace DirectX;

constexpr auto MOVE_SPEED  = 10.0f;
constexpr auto MOUSE_SPEED = 0.0005f;

Camera::Camera() :
    m_position(XMFLOAT3(0.0f, 0.0f, -5.0f)),
    m_rotation(0.0f, 0.0f)
{
}

void Camera::Update(const float deltaTime)
{
    XMMATRIX rotationMatrix = XMMatrixRotationX(m_rotation.x);
    rotationMatrix = XMMatrixMultiply(rotationMatrix, XMMatrixRotationY(m_rotation.y));

    auto translation = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    if (Window::IsKeyPressed('W'))
        translation = XMFLOAT4(translation.x, translation.y, translation.z + 1.0f, translation.w);

    if (Window::IsKeyPressed('A'))
        translation = XMFLOAT4(translation.x - 1.0f, translation.y, translation.z, translation.w);

    if (Window::IsKeyPressed('S'))
        translation = XMFLOAT4(translation.x, translation.y, translation.z - 1.0f, translation.w);

    if (Window::IsKeyPressed('D'))
        translation = XMFLOAT4(translation.x + 1.0f, translation.y, translation.z, translation.w);

    const auto loadedTranslation = XMLoadFloat4(&translation);
    const auto transformedTranslation = XMVector4Transform(loadedTranslation, rotationMatrix);
    const auto normalizedTranslation = XMVector3Normalize(transformedTranslation);
    const auto speedVec = normalizedTranslation * MOVE_SPEED * deltaTime;
    XMStoreFloat4(&translation, speedVec);

    m_position = XMFLOAT3(m_position.x + translation.x, m_position.y + translation.y, m_position.z + translation.z);
}

void Camera::MouseMoved(const float diffX, const float diffY)
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