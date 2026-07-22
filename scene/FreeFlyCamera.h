#pragma once
#include "../system/commontypes.h"
#include "../system/Camera.h"

// WASD移動＋マウス右ドラッグ視点のフリーフライカメラ
// 入力取得〜Cameraへの反映まで自己完結する。
class FreeFlyCamera
{
public:
	// pos を設定し、lookat 方向から yaw/pitch を算出する（モード切替時のハンドオフ用）
	void AdoptFromLookAt(const Vector3& pos, const Vector3& lookat);

	// CDirectInput を読み、移動・視点を更新して cam に反映する
	void Update(float dt, Camera& cam);

	Vector3 GetPosition() const { return m_pos; }

private:
	Vector3 forward() const;	// yaw/pitch → 前方ベクトル

	float   m_yaw   = 0.0f;			// 水平回転（ラジアン）
	float   m_pitch = 0.0f;			// 垂直回転（ラジアン）
	Vector3 m_pos{ 0, 0, -300 };	// カメラ位置
	int     m_prevMouseX = 0;		// 前フレームのマウスX座標
	int     m_prevMouseY = 0;		// 前フレームのマウスY座標
	bool    m_dragging = false;		// 右ドラッグ中か
	float   m_moveSpeed = 200.0f;	// 移動速度（単位/秒）
	float   m_lookSpeed = 0.005f;	// 回転感度（ラジアン/ピクセル）
};
