#pragma once

#include "PCH.h"
#include "Helper.h"

using namespace DirectX;

class SimpleCamera
{
public:
	SimpleCamera();

	void Init(XMFLOAT3 position);
	void Update(float elapsedSecond);

	//BoundingFrustum GetFrustum() const;
	BoundingFrustum GetFrustum(float fov, float aspectRatio, float nearPlane=1.0, float farPlane=1000.f) const;
	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix(float fov, float aspectRatio, float nearPlane=1.0, float farPlane=1000.f) const;
	void SetMoveSpeed(float unitPerSecond);
	void SetTurnSpeed(float radiansPerSecond);

	void OnKeyDown(SDL_KeyboardEvent& key);
	void OnKeyUp(SDL_KeyboardEvent& key);

private:
	void Reset();

	struct KeysPressed
	{
		bool w;
		bool s;
		bool a;
		bool d;

		bool left;
		bool right;
		bool up;
		bool down;
	};

	XMFLOAT3	m_initialPosition;
	XMFLOAT3	m_position;
	float	m_yaw;
	float	m_pitch;
	XMFLOAT3	m_lookDirection;
	XMFLOAT3	m_upDirection;
	float	m_moveSpeed;
	float	m_turnSpeed;

	KeysPressed m_keysPressed;
};
