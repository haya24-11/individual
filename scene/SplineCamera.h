#pragma once
#include <vector>
#include <memory>
#include "../system/commontypes.h"
#include "../system/Camera.h"
#include "../system/C3DShape.h"


struct CameraTake;

// スプライン曲線に沿ってカメラを動かすクラス
// 経路（制御点・区間速度）＋弧長パラメータ化（等速化）＋再生＋可視化＋編集UIを保持する。
class SplineCamera
{
public:
	// 位置・速度・ロールの補間方法。
	// 曲線のコードは消さずにモードで切り替えるので、いつでも元に戻せる。
	enum class Interp { CatmullRom, Linear, None };

	void Init();						// 既定の制御点セット＋マーカー生成
	void ResetDefault();				// 既定4点・速度・進行量をリセット
	void Update(float dt, Camera& cam);	// active時：距離を進めて t を求め、cam に反映
	void DrawVisualization();			// 経路（黄）＋現在地(赤)/注視点(青)/制御点(緑)を描画
	// UI（Begin/End は呼ばない。FightScene 側のタブの中に描かれる）
	void DrawPlaybackUI();				// 再生設定（有効・ループ・注視点）
	void DrawCurveEditorUI();			// 曲線エディター（2Dキャンバス・制御点）
	void DrawSpeedUI();					// 速度（基準速度・ノブ・グラフ）
	void DrawRollUI();					// ロール（ダッチアングル)

	void CaptureTo(CameraTake& take, const Vector3& origin)const;
	bool RestoreFrom(const CameraTake& take, const Vector3& origin);



	bool     IsActive() const { return m_active; }
	void     SetActive(bool a) { m_active = a; }
	void Play();		// 先頭から再生し直す（終端で止まっていても巻き戻す）
	Vector3  CurrentPos() const { return Eval(m_t); }	// ハンドオフ用（現在のカメラ位置）
	Vector3& Lookat() { return m_lookat; }				// ギズモ編集ターゲット
	std::vector<Vector3>& Points() { return m_points; }	// ギズモ編集ターゲット
	int      PointCount() const { return (int)m_points.size(); }

	// スプラインの数学
	Vector3 Eval(float u) const;		// u位置のカメラ座標（Catmull-Rom）
	float   SpeedAt(float u) const;		// u位置の速度倍率（隣接点間を線形補間）
	float   RollAt(float u) const;		// u位置の roll[度]（隣接4点をCatmull-Rom補間）
	void    BuildArcTable();			// 弧長テーブル構築
	float   ArcLengthToU(float s) const;// 距離→u 逆引き
	float   ArcLengthAtU(float u) const;// u→距離（ArcLengthToU の逆）

private:
	// 平面(axisH, axisV)=(0:X,1:Y,2:Z) を1枚のキャンバスとして描画・編集する
	// allowAddRemove: このビューで空クリック追加・右クリック削除を許可するか
	void DrawEditCanvas(const char* id, int axisH, int axisV, bool allowAddRemove);

	// 可視化専用。補間なしのときも経路が見えるよう、直線でつないで返す
	Vector3 EvalForDraw(float u) const;

	std::vector<Vector3> m_points;		// 制御点
	std::vector<float>   m_speeds;		// 制御点ごとの速度倍率
	std::vector<float>   m_rolls;		// 制御点ごとの roll（度）。ダッチアングル
	Vector3 m_lookat{ 0, 0, 0 };		// 固定注視点

	bool  m_active   = false;			// 再生中か
	bool  m_loop     = true;			// 経路をループ（閉曲線）
	bool  m_showPath = true;			// 経路を可視化
	Interp m_interp  = Interp::CatmullRom;	// 補間方法（比較したいときは Linear / None に切り替え）
	bool  m_useArcLength = true;		// 弧長パラメータ化。OFF で u を直接進める（速度がばらつく）
	float m_t     = 0.0f;				// 進行パラメータ（u）
	float m_speed = 300.0f;				// マスター速度（ワールド単位/秒）
	float m_dist  = 0.0f;				// 進行距離アキュムレータ

	std::vector<float> m_arc;			// 累積距離テーブル
	float m_totalLen = 0.0f;			// 経路の総距離
	int   m_samplesPerSeg = 128;		// 1区間の分割数（32 だと速度が±2%ほど揺れる）

	// ImGui 2Dキャンバス編集用
	float m_editViewHalf = 600.0f;		// キャンバスに映すワールド半径（±600）
	int   m_canvasDrag   = -1;			// ドラッグ中の制御点index（-1=なし）
	int   m_canvasDragView = -1;		// ドラッグ中キャンバスの縦軸(axisV)。どちらの面がドラッグ中か区別する
	int   m_speedDrag    = -1;			// 速度グラフでドラッグ中の制御点index（-1=なし）

	// --- 等速の確認（デバッグ計測）---
	// 毎フレーム「実際に動いた距離 ÷ 経過時間」を測り、目標速度との比を記録する。
	// 弧長パラメータ化が効いていれば比は常に 1.00 付近に張り付く
	static constexpr int kSpeedHistory = 240;	// 履歴の長さ（60FPSで約4秒）
	float   m_speedHist[kSpeedHistory] = {};	// 実測速度 ÷ 目標速度 のリングバッファ
	int     m_speedHistPos   = 0;				// 次に書き込む位置
	int     m_speedHistCount = 0;				// 溜まっている個数
	Vector3 m_measPrevPos{};					// 前フレームのカメラ位置
	bool    m_measValid  = false;				// 前フレームの位置を持っているか
	float   m_measSpeed  = 0.0f;				// 直近の実測速度（単位/秒）
	float   m_measTarget = 0.0f;				// 直近の目標速度（単位/秒）

	void ResetSpeedMeasure();					// 計測をやり直す（設定を切り替えたとき）
	void DrawSpeedMeasureUI();					// 計測結果の表示

	std::unique_ptr<Sphere> m_marker;		// 現在地点（赤）
	std::unique_ptr<Sphere> m_lookatMarker;	// 注視点（青）
	std::unique_ptr<Sphere> m_pointMarker;	// 制御点（緑）
};
