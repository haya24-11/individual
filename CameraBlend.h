#pragma once

#include "CameraPose.h"


class CameraBlend
{
public:
	void  Start(
		const CameraPose& from,
		float duration);
	CameraPose Update(
		float dt,
		const CameraPose& target);

	bool IsActive()const
	{
		return m_active;
	}

private:
	CameraPose m_from{};

	float m_elapsed = 0.0f;
	float m_duration = 0.8f;

	bool m_active = false;


};

