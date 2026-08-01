#include "FightingCamera.h"
#include "../system/imgui/imgui.h"
#include <algorithm>
#include <cmath>

void FightingCamera::Update(float dt, Camera& cam, const Vector3& p1, const Vector3& p2)
{
	// 水平中心（キャラは主にX軸で離れるので水平面で中点をとる）
	float cx = (p1.x + p2.x) * 0.5f;
	float cz = (p1.z + p2.z) * 0.5f;

	// 間合い（水平距離）
	float dx  = p1.x - p2.x;
	float dz  = p1.z - p2.z;
	float sep = sqrtf(dx * dx + dz * dz);

	// 間合いが広いほど引く（両者を画に収める）
	float dist = std::clamp(m_baseDist + sep * m_distFactor, m_minDist, m_maxDist);

	// 目標の位置・注視点（-Z前方から正面を映す）
	Vector3 target(cx, m_camHeight, cz - dist);
	Vector3 look(cx, m_lookHeight, cz);

	if (!m_init)
	{
		// モード復帰・初回は即スナップ（飛ばさない）
		m_pos  = target;
		m_look = look;
		m_init = true;
	}
	else
	{
		// 指数スムージング（フレームレート非依存）
		float a = 1.0f - expf(-m_smooth * dt);
		m_pos  = Vector3::Lerp(m_pos,  target, a);
		m_look = Vector3::Lerp(m_look, look,   a);
	}

	cam.SetPosition(m_pos);
	cam.SetLookat(m_look);
	cam.SetUP(Vector3(0, 1, 0));
}

void FightingCamera::DebugUI()
{
	ImGui::Begin("Fighting Camera");

	ImGui::SliderFloat("Cam Height",  &m_camHeight,  -100.0f, 300.0f);
	ImGui::SliderFloat("Look Height", &m_lookHeight, -100.0f, 300.0f);
	ImGui::Separator();
	ImGui::SliderFloat("Base Dist",   &m_baseDist,   50.0f, 1000.0f);
	ImGui::SliderFloat("Dist Factor", &m_distFactor, 0.0f,  3.0f);
	ImGui::SliderFloat("Min Dist",    &m_minDist,    50.0f, 1000.0f);
	ImGui::SliderFloat("Max Dist",    &m_maxDist,    100.0f, 2000.0f);
	ImGui::Separator();
	ImGui::SliderFloat("Smooth",      &m_smooth,     1.0f,  30.0f);

	ImGui::End();
}
