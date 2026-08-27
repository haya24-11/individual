
#include "CameraBlend.h"

#include <algorithm>


void CameraBlend::Start(const CameraPose& from, float duration)
{
	//ブレンド開始地点を固定
	m_from = from;

	//経過時間を先頭へ戻す
	m_elapsed = 0.0f;

	m_duration =
		std::max(duration, 0.0001f);
	//ブレンド開始
	m_active = true;



}

CameraPose CameraBlend::Update(float dt, const CameraPose& target)
{
	if (!m_active)
	{
		return target;
	}

	m_elapsed += dt;

	float t =
		m_elapsed / m_duration;

	t = std::clamp(t, 0.0f, 1.0f);

	float easedT =
		CameraSmootherStep(t);

	CameraPose result =
		LerpCameraPose(
			m_from,
			target,
			easedT);

	// 最後まで進んだら終了
	if (t >= 1.0f)
	{
		m_active = false;

		// 誤差を残さず目標姿勢に一致させる
		result = target;
	}

	return result;
}

