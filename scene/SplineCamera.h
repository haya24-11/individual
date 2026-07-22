#pragma once
#include <vector>
#include <memory>
#include "../system/commontypes.h"
#include "../system/Camera.h"
#include "../system/C3DShape.h"

// スプライン曲線に沿ってカメラを動かすクラス
// 経路（制御点・区間速度）＋弧長パラメータ化（等速化）＋再生＋可視化＋編集UIを保持する。
class SplineCamera
{
public:
	void Init();						// 既定の制御点セット＋マーカー生成
	void ResetDefault();				// 既定4点・速度・進行量をリセット
	void Update(float dt, Camera& cam);	// active時：距離を進めて t を求め、cam に反映
	void DrawVisualization();			// 経路（黄）＋現在地(赤)/注視点(青)/制御点(緑)を描画
	void DebugUI();						// ImGui「Spline Camera」

	bool     IsActive() const { return m_active; }
	void     SetActive(bool a) { m_active = a; }
	Vector3  CurrentPos() const { return Eval(m_t); }	// ハンドオフ用（現在のカメラ位置）
	Vector3& Lookat() { return m_lookat; }				// ギズモ編集ターゲット
	std::vector<Vector3>& Points() { return m_points; }	// ギズモ編集ターゲット
	int      PointCount() const { return (int)m_points.size(); }

	// スプラインの数学
	Vector3 Eval(float u) const;		// u位置のカメラ座標（Catmull-Rom）
	float   SpeedAt(float u) const;		// u位置の速度倍率（隣接点間を線形補間）
	void    BuildArcTable();			// 弧長テーブル構築
	float   ArcLengthToU(float s) const;// 距離→u 逆引き

private:
	// 平面(axisH, axisV)=(0:X,1:Y,2:Z) を1枚のキャンバスとして描画・編集する
	// allowAddRemove: このビューで空クリック追加・右クリック削除を許可するか
	void DrawEditCanvas(const char* id, int axisH, int axisV, bool allowAddRemove);

	std::vector<Vector3> m_points;		// 制御点
	std::vector<float>   m_speeds;		// 制御点ごとの速度倍率
	Vector3 m_lookat{ 0, 0, 0 };		// 固定注視点

	bool  m_active   = false;			// 再生中か
	bool  m_loop     = true;			// 経路をループ（閉曲線）
	bool  m_showPath = true;			// 経路を可視化
	float m_t     = 0.0f;				// 進行パラメータ（u）
	float m_speed = 300.0f;				// マスター速度（ワールド単位/秒）
	float m_dist  = 0.0f;				// 進行距離アキュムレータ

	std::vector<float> m_arc;			// 累積距離テーブル
	float m_totalLen = 0.0f;			// 経路の総距離
	int   m_samplesPerSeg = 32;			// 1区間の分割数

	// ImGui 2Dキャンバス編集用
	float m_editViewHalf = 600.0f;		// キャンバスに映すワールド半径（±600）
	int   m_canvasDrag   = -1;			// ドラッグ中の制御点index（-1=なし）
	int   m_canvasDragView = -1;		// ドラッグ中キャンバスの縦軸(axisV)。どちらの面がドラッグ中か区別する

	std::unique_ptr<Sphere> m_marker;		// 現在地点（赤）
	std::unique_ptr<Sphere> m_lookatMarker;	// 注視点（青）
	std::unique_ptr<Sphere> m_pointMarker;	// 制御点（緑）
};
