#pragma once
#include <vector>
#include "../system/commontypes.h"
#include "../system/Camera.h"

// カメラワークのテイク録画・再生
// メインカメラの姿勢（位置・注視点・up）を毎フレーム記録し、再生時はその記録で上書きする。
// 誰がカメラを動かしていても記録できる（手動フリー操作・スプライン再生など）。
class CameraRecorder
{
public:
	void Sample(const Camera& cam, float dt);	// 録画中：現在姿勢を1フレーム分追加
	void ApplyPlayback(Camera& cam, float dt);	// 再生中：時間補間して cam に反映
	void DrawUI();		// Begin/End は呼ばない（CarScene のタブ内に描く）

	bool IsRecording() const { return m_recording; }
	bool IsPlaying()   const { return m_playing; }

private:
	// 1フレーム分のカメラ姿勢＋記録時刻
	struct CamKey { float t; Vector3 pos, look, up; };

	void StartRecord();	// keys消去→録画開始
	void StartPlay();	// 先頭から再生開始（keyが2つ以上必要）
	void Save(const char* path);	// テキストで永続化
	void Load(const char* path);

	std::vector<CamKey> m_keys;			// 記録したキーフレーム（時刻昇順）
	bool  m_recording = false;
	bool  m_playing   = false;
	bool  m_loop      = false;			// 再生をループ
	float m_recTime   = 0.0f;			// 録画の経過時刻
	float m_playTime  = 0.0f;			// 再生の経過時刻
};
