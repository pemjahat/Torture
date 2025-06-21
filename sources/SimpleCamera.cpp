#include "SimpleCamera.h"
#include <algorithm>

SimpleCamera::SimpleCamera():
	m_initialPosition(0, 0, 0),
	m_position(m_initialPosition),
	m_yaw(XM_PI),
	m_pitch(0.f),
	m_lookDirection(0, 0, -1),
	m_upDirection(0, 1, 0),
	m_moveSpeed(20.f),
	m_turnSpeed(XM_PIDIV2),
	m_keysPressed{}
{ }

void SimpleCamera::Init(XMFLOAT3 position)
{
	m_initialPosition = position;
	Reset();
}

void SimpleCamera::SetMoveSpeed(float unitsPerSecond)
{
	m_moveSpeed = unitsPerSecond;
}

void SimpleCamera::SetTurnSpeed(float radiansPerSecond)
{
	m_turnSpeed = radiansPerSecond;
}

void SimpleCamera::Reset()
{
	m_position = m_initialPosition;
	m_yaw = XM_PI;
	m_pitch = 0.f;
	m_lookDirection = { 0, 0, -1 };
}

void SimpleCamera::Update(float elapsedSeconds)
{
	// Calculate move vector in camera space
	XMFLOAT3 move(0, 0, 0);

	if (m_keysPressed.a)
		move.x -= 1.f;
	if (m_keysPressed.d)
		move.x += 1.f;
	if (m_keysPressed.w)
		move.z += 1.f;
	if (m_keysPressed.s)
		move.z -= 1.f;

	if (fabs(move.x) > 0.1 || fabs(move.z) > 0.1)
	{
		XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
		move.x = XMVectorGetX(vector);
		move.z = XMVectorGetZ(vector);
	}

	float moveInterval = m_moveSpeed * elapsedSeconds;
	float rotateInterval = m_turnSpeed * elapsedSeconds;

	if (m_keysPressed.left)
		m_yaw += rotateInterval;
	if (m_keysPressed.right)
		m_yaw -= rotateInterval;
	if (m_keysPressed.up)
		m_pitch += rotateInterval;
	if (m_keysPressed.down)
		m_pitch -= rotateInterval;

	// prevent looking too far up or down
	m_pitch = std::min(m_pitch, (XM_PIDIV4 + XM_1DIVPI));
	m_pitch = std::max(-(XM_PIDIV4 + XM_1DIVPI), m_pitch);

	// determine look direction
	float r = cosf(m_pitch);
	m_lookDirection.x = r * sinf(m_yaw);
	m_lookDirection.y = sinf(m_pitch);
	m_lookDirection.z = r * cosf(m_yaw);

	XMVECTOR right = XMVector3Cross(XMLoadFloat3(&m_lookDirection), XMLoadFloat3(&m_upDirection));
	right = XMVector3Normalize(right);

	// Update camera move based on Look + Right direction
	XMVECTOR pos = XMLoadFloat3(&m_position);
	pos += XMLoadFloat3(&m_lookDirection) * move.z * moveInterval;
	pos += right * move.x * moveInterval;
	XMStoreFloat3(&m_position, pos);
}

XMMATRIX SimpleCamera::GetViewMatrix()
{
	return XMMatrixLookToRH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_lookDirection), XMLoadFloat3(&m_upDirection));
}

XMMATRIX SimpleCamera::GetProjectionMatrix(float fov, float aspectRatio, float nearPlane, float farPlane)
{
	return XMMatrixPerspectiveFovRH(fov, aspectRatio, nearPlane, farPlane);		   
}

void SimpleCamera::OnKeyDown(SDL_KeyboardEvent& key)
{
	switch (key.keysym.sym)
	{
	case SDLK_w:
		m_keysPressed.w = true;
		break;
	case SDLK_a:
		m_keysPressed.a = true;
		break;
	case SDLK_s:
		m_keysPressed.s = true;
		break;
	case SDLK_d:
		m_keysPressed.d = true;
		break;
	case SDLK_LEFT:
		m_keysPressed.left = true;
		break;
	case SDLK_RIGHT:
		m_keysPressed.right = true;
		break;
	case SDLK_UP:
		m_keysPressed.up = true;
		break;
	case SDLK_DOWN:
		m_keysPressed.down = true;
		break;
	}
}

void SimpleCamera::OnKeyUp(SDL_KeyboardEvent& key)
{
	switch (key.keysym.sym)
	{
	case SDLK_w:
		m_keysPressed.w = false;
		break;
	case SDLK_a:
		m_keysPressed.a = false;
		break;
	case SDLK_s:
		m_keysPressed.s = false;
		break;
	case SDLK_d:
		m_keysPressed.d = false;
		break;
	case SDLK_LEFT:
		m_keysPressed.left = false;
		break;
	case SDLK_RIGHT:
		m_keysPressed.right = false;
		break;
	case SDLK_UP:
		m_keysPressed.up = false;
		break;
	case SDLK_DOWN:
		m_keysPressed.down = false;
		break;
	}
}