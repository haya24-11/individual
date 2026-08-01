#include "FreeFlyCamera.h"
#include "../system/CDirectInput.h"
#include "../system/imgui/imgui.h"
#include <cmath>

// yaw/pitch から前方ベクトルを求める（左手座標系、yaw=Y軸回り・pitch=X軸回り）
Vector3 FreeFlyCamera::forward() const
{
	Vector3 f{
		cosf(m_pitch) * sinf(m_yaw),
		sinf(m_pitch),
		cosf(m_pitch) * cosf(m_yaw)
	};
	f.Normalize();
	return f;
}

// pos を設定し、注視点方向から yaw/pitch を復元する（切替時に向きを引き継ぐ）
void FreeFlyCamera::AdoptFromLookAt(const Vector3& pos, const Vector3& lookat)
{
	m_pos = pos;

	Vector3 f = lookat - pos;
	f.Normalize();
	m_yaw   = atan2f(f.x, f.z);
	m_pitch = asinf(f.y);
}

void FreeFlyCamera::Update(float dt, Camera& cam)
{
	CDirectInput& in = CDirectInput::GetInstance();

	// --- マウス右ドラッグで視点回転 ---
	// ImGuiがマウスを使っている間（UI操作中）は回転しない
	bool uiCapturingMouse = ImGui::GetIO().WantCaptureMouse;

	if (in.GetMouseRButtonCheck() && !uiCapturingMouse)
	{
		int curX = in.GetMousePosX();
		int curY = in.GetMousePosY();

		if (!m_dragging)
		{
			// 押し始めは基準座標をセット（飛び防止）
			m_prevMouseX = curX;
			m_prevMouseY = curY;
			m_dragging = true;
		}
		else
		{
			int dx = curX - m_prevMouseX;
			int dy = curY - m_prevMouseY;

			m_yaw   += dx * m_lookSpeed;
			m_pitch -= dy * m_lookSpeed;

			m_prevMouseX = curX;
			m_prevMouseY = curY;
		}
	}
	else
	{
		m_dragging = false;
	}

	// ピッチを真上・真下手前でクランプ（破綻防止）
	const float pitchLimit = PI / 2.0f - 0.01f;
	if (m_pitch >  pitchLimit) m_pitch =  pitchLimit;
	if (m_pitch < -pitchLimit) m_pitch = -pitchLimit;

	// --- 前方／右ベクトル算出 ---
	Vector3 fwd = forward();

	Vector3 worldUp{ 0, 1, 0 };
	Vector3 right = worldUp.Cross(fwd);	// LH: up × forward = right
	right.Normalize();

	// --- WASDで移動、Q/E(Space)で上下移動 ---
	float speed = m_moveSpeed;
	if (in.CheckKeyBuffer(DIK_LSHIFT)) speed *= 3.0f;	// Shiftで増速
	float dist = speed * dt;

	if (in.CheckKeyBuffer(DIK_W)) m_pos += fwd * dist;
	if (in.CheckKeyBuffer(DIK_S)) m_pos -= fwd * dist;
	if (in.CheckKeyBuffer(DIK_D)) m_pos += right * dist;
	if (in.CheckKeyBuffer(DIK_A)) m_pos -= right * dist;
	if (in.CheckKeyBuffer(DIK_E) || in.CheckKeyBuffer(DIK_SPACE)) m_pos += worldUp * dist;
	if (in.CheckKeyBuffer(DIK_Q)) m_pos -= worldUp * dist;

	// --- カメラへ反映 ---
	cam.SetPosition(m_pos);
	cam.SetLookat(m_pos + fwd);
}

// ImGuiの入力でカメラを操作する（別OSウィンドウでも効く。DirectInputのフォアグラウンド問題を回避）
void FreeFlyCamera::UpdateImGui(Camera& cam, bool active)
{
	ImGuiIO& io = ImGui::GetIO();
	float dt = io.DeltaTime;

	if (active)
	{
		// 右ドラッグで視点回転（ImGuiのマウスデルタを使用）
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			m_yaw   += io.MouseDelta.x * m_lookSpeed;
			m_pitch -= io.MouseDelta.y * m_lookSpeed;
		}

		// ピッチをクランプ
		const float pitchLimit = PI / 2.0f - 0.01f;
		if (m_pitch >  pitchLimit) m_pitch =  pitchLimit;
		if (m_pitch < -pitchLimit) m_pitch = -pitchLimit;

		Vector3 fwd = forward();
		Vector3 worldUp{ 0, 1, 0 };
		Vector3 right = worldUp.Cross(fwd);
		right.Normalize();

		float speed = m_moveSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
		float dist = speed * dt;

		if (ImGui::IsKeyDown(ImGuiKey_W)) m_pos += fwd   * dist;
		if (ImGui::IsKeyDown(ImGuiKey_S)) m_pos -= fwd   * dist;
		if (ImGui::IsKeyDown(ImGuiKey_D)) m_pos += right * dist;
		if (ImGui::IsKeyDown(ImGuiKey_A)) m_pos -= right * dist;
		if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) m_pos += worldUp * dist;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) m_pos -= worldUp * dist;
	}

	cam.SetPosition(m_pos);
	cam.SetLookat(m_pos + forward());
}
