#pragma once

#include <algorithm>
#include "system/commontypes.h"


struct CameraPose
{
	Vector3 position{ 0,0,-300 };
	Vector3 lookat{ 0,0,0 };
	Vector3 up{ 0,1,0 };

};

inline CameraPose LerpCameraPose(
	const CameraPose& from,
	const CameraPose& to,
	float t)
{
	t = std::clamp(t, 0.0f, 1.0f);


	CameraPose result;

	result.position =
		Vector3::Lerp(from.position, to.position, t);

	result.lookat =
		Vector3::Lerp(from.lookat, to.lookat, t);

	result.up =
		Vector3::Lerp(from.up, to.up, t);


	if (result.up.LengthSquared()>0.00001f)
	{
		result.up.Normalize();
	}
	else
	{
		result.up = Vector3(0, 1, 0);
	}

	return result;

}

inline float CameraSmootherStep(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);

	return t * t * t
		* (t * (t * 6.0f - 15.0f) + 10.0f);

}