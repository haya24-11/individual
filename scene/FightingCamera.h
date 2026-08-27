#pragma once
#include "../system/commontypes.h"
#include "../system/Camera.h"

// 格闘ゲームの基本カメラ
// 2キャラの中点を正面から追従し、間合い（2キャラ間距離）に応じて引く（ドリーでズーム）。
// 位置・注視点はスムージングして追従する。projection(FOV)は触らず Camera へ反映するだけ。
class FightingCamera
{
public:
	// 2キャラ位置から目標を計算し、スムージングして cam に反映する
	void Update(float dt, Camera& cam, const Vector3& p1, const Vector3& p2);

	// パラメータ調整UI（ImGui「Fighting Camera」）
	void DrawUI();		// Begin/End は呼ばない（CarScene のタブ内に描く）

	// 次フレームで目標へ即スナップ（モード復帰時のジャンプ防止）
	void ResetSnap() { m_init = false; }

private:
	// --- 調整パラメータ（ImGuiで編集） ---
	float m_camHeight  = 20.0f;		// カメラのワールドY
	float m_lookHeight = 0.0f;		// 注視点のワールドY（キャラ中心あたり）
	float m_baseDist   = 250.0f;	// 最小の引き距離（-Z方向）
	float m_distFactor = 0.8f;		// 間合い1単位あたり追加で引く量
	float m_minDist    = 200.0f;	// 引き距離の下限
	float m_maxDist    = 900.0f;	// 引き距離の上限
	float m_smooth     = 8.0f;		// 追従の減衰（大きいほど速く追従）

	// --- 状態 ---
	Vector3 m_pos{ 0, 20, -300 };	// 現在のカメラ位置（スムージング済み）
	Vector3 m_look{ 0, 0, 0 };		// 現在の注視点（スムージング済み）
	bool    m_init = false;			// 初回スナップ判定（false=次フレームで即スナップ）
};
