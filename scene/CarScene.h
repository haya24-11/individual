#pragma once
#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include "../system/commontypes.h"
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/Camera.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"
#include "../system/CSprite.h"
#include "../system/CAnimationMeshBlender.h"
#include "../system/CAnimationData.h"
#include "../system/BoneCombMatrix.h"
#include "FreeFlyCamera.h"
#include "SplineCamera.h"
#include "FightingCamera.h"
#include "CameraRecorder.h"

class CarScene : public IScene
{
	// 現在の姿勢を表すクォータニオン
	Quaternion m_RotationQ{};

	// 現在の姿勢を表す行列
	Matrix4x4 m_RotationMtx{};

	Matrix4x4 m_ScaleMtx{};
public:
	explicit CarScene();
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;
	void debugRubikCubeLocalRotation();
	void drawMainToolsUI();		// 全機能を1つのウィンドウにまとめて描く

	void debugModelSelect(const char* title, int& selectedIndex, std::string& meshId);
	std::string ensureModelLoaded(int index);	// 未ロードならLoad+Registerしてmeshidを返す

	// 制御点・注視点を動かす軸ギズモ
	enum class GizmoAxis { None, X, Y, Z };
	enum class PickKind  { None, ControlPoint, Lookat };

	bool     computeMouseRay(Vector3& outOrigin, Vector3& outDir) const;
	Vector3* getPickTarget(PickKind kind, int index);	// 選択対象の Vector3*（無効ならnullptr）
	void     updateGizmoPicking();		// クリック選択・ドラッグ処理（update()から呼ぶ）
	void     drawGizmo();				// 選択中なら3軸の矢印を描画（draw()から呼ぶ）

private:
	Camera m_camera;									// 固定カメラ
	std::array<std::unique_ptr<Segment>,3> m_segments;	// ローカル軸表示用線分

	// 板ポリ（テクスチャ付き矩形ポリゴン）
	std::unique_ptr<CSprite> m_sprite;

	// カメラ（メイン: 格ゲー基本⇔スプライン。Free-flyはデバッグ用の別ビュー）
	enum class CamMode { Fighting, Spline };
	CamMode        m_camMode = CamMode::Fighting;	// 既定は基本カメラ
	FreeFlyCamera  m_freeCam;						// デバッグ第2ビューを操作する
	SplineCamera   m_splineCam;						// 曲線カメラ
	FightingCamera m_fightCam;						// 格ゲー基本カメラ

	// カメラワークの録画・再生／手動フリー操作（メイン画面）
	CameraRecorder m_recorder;						// テイク録画・再生
	FreeFlyCamera  m_manualFly;						// 手動フリー操作（メイン・DirectInput）
	bool m_manualCam     = false;					// 手動でメインカメラを飛ばす
	bool m_prevManualCam = false;					// 立ち上がり検出

	// デバッグ第2ビュー（free-flyの映像を別ウィンドウにオフスクリーン描画して表示）
	Camera m_debugCam;								// free-flyが反映する専用カメラ
	bool   m_debugViewOpen = false;					// 別ウィンドウ表示 ON/OFF（ImGui）
	UINT   m_dbgW = 0, m_dbgH = 0;					// オフスクリーン解像度
	ComPtr<ID3D11Texture2D>          m_dbgColorTex;
	ComPtr<ID3D11RenderTargetView>   m_dbgRTV;
	ComPtr<ID3D11ShaderResourceView> m_dbgSRV;
	ComPtr<ID3D11Texture2D>          m_dbgDepthTex;
	ComPtr<ID3D11DepthStencilView>   m_dbgDSV;

	void createDebugTarget();	// オフスクリーンRT生成（init）
	void drawSceneGeometry();	// 3D描画本体（メイン/第2ビュー共通）
	void renderDebugView();		// free-fly視点でオフスクリーンへ再描画

	// 今表示しているメッシュのID
	std::string m_meshid{};		// player1 の表示メッシュID
	std::string m_meshid2{};	// player2 の表示メッシュID
	int m_p1Select = 14;			// Player1 のコンボ選択index（初期表示: suzu.pmx）
	int m_p2Select = 0;			// Player2 のコンボ選択index
	float m_p1Scale = 1.0f;		// Player1(suzu/PMX) 専用スケール。MMDは約20単位なので約9倍でX Bot相当

	// 各プレイヤーの配置（向かい合わせ。値は実機で微調整）
	Vector3 m_p1Pos{ -150, -100, 0 };	// player1 は左（原点が足元→地面Y=-100に接地）
	Vector3 m_p2Pos{  150, 0, 0 };	// player2 は右
	float   m_p1FaceY = -PI / 2.0f;	// 相手(+X方向)を向く ※モデルの正面軸により要調整
	float   m_p2FaceY =  PI / 2.0f;	// 相手(-X方向)を向く ※同上

	// 軸ギズモ（制御点・注視点をマウスで移動）
	PickKind  m_selectedKind = PickKind::None;	// 現在選択中の対象種別
	int       m_selectedIndex = -1;				// ControlPoint時のインデックス
	GizmoAxis m_dragAxis = GizmoAxis::None;		// ドラッグ中の軸（None=非ドラッグ）

	std::unique_ptr<Cylinder> m_gizmoShaft;		// 矢印の軸（共有・使い回し）
	std::unique_ptr<Cone>     m_gizmoHead;		// 矢印の先端（共有・使い回し）

	float m_gizmoShaftLen   = 60.0f;
	float m_gizmoShaftRad   = 3.0f;
	float m_gizmoHeadLen    = 20.0f;
	float m_gizmoHeadRad    = 8.0f;
	float m_gizmoPickThresh = 8.0f;	// 軸ドラッグの当たり判定しきい値（ワールド単位）

	// Player1 アニメーション（アッパーカット試作）
	std::unique_ptr<CAnimationMeshBlender> m_p1AnimMesh;	// X Bot スキンメッシュ
	std::unique_ptr<CAnimationData>        m_uppercutData;	// モーションFBX保持
	BoneCombMatrix m_p1BoneComb;		// ボーン行列定数バッファ(b5)
	float m_p1AnimFrame  = 0.0f;		// 再生フレーム
	bool  m_p1Attacking  = false;		// 攻撃モーション再生中か
	int   m_uppercutFrames = 0;			// モーションの総キー数
	float m_animFps = 30.0f;			// 再生速度（キー/秒）

};

REGISTER_CLASS(CarScene)