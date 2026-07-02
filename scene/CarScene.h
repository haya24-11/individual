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

class CarScene : public IScene
{
	// 回転角度
	Vector3 m_Rotation{};

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
	void debugRubikCubeRotation();
	void debugRubikCubeLocalRotation();

	void debugModelSelect(const char* title, int& selectedIndex, std::string& meshId);
	std::string ensureModelLoaded(int index);	// 未ロードならLoad+Registerしてmeshidを返す

	// スプラインカメラ
	Vector3 evalSpline(float u) const;			// u位置のカメラ座標（loop/open対応）
	void    resetSplineDefault();				// 既定の制御点をセット
	void    debugSplineCamera();				// ImGui編集UI

	// 制御点・注視点を動かす軸ギズモ
	enum class GizmoAxis { None, X, Y, Z };
	enum class PickKind  { None, ControlPoint, Lookat };

	bool     computeMouseRay(Vector3& outOrigin, Vector3& outDir) const;
	Vector3* getPickTarget(PickKind kind, int index);	// 選択対象の Vector3*（無効ならnullptr）
	void     updateGizmoPicking();		// クリック選択・ドラッグ処理（update()から呼ぶ）
	void     drawGizmo();				// 選択中なら3軸の矢印を描画（draw()から呼ぶ）

private:
	Camera m_camera;									// 固定カメラ
	std::unique_ptr<Box> m_shapecube;					// 立方体
	std::array<std::unique_ptr<Segment>,3> m_segments;	// ローカル軸表示用線分

	// 板ポリ（テクスチャ付き矩形ポリゴン）
	std::unique_ptr<CSprite> m_sprite;

	// フリーフライ(FPS)カメラ用の状態
	float   m_yaw   = 0.0f;			// 水平回転（ラジアン）
	float   m_pitch = 0.0f;			// 垂直回転（ラジアン）
	Vector3 m_camPos{ 0, 0, -300 };	// カメラ位置
	int     m_prevMouseX = 0;		// 前フレームのマウスX座標
	int     m_prevMouseY = 0;		// 前フレームのマウスY座標
	bool    m_dragging = false;		// 右ドラッグ中か
	float   m_moveSpeed = 200.0f;	// 移動速度（単位/秒）
	float   m_lookSpeed = 0.005f;	// 回転感度（ラジアン/ピクセル）

	// 今表示しているメッシュのID
	std::string m_meshid{};		// player1 の表示メッシュID
	std::string m_meshid2{};	// player2 の表示メッシュID
	int m_p1Select = 0;			// Player1 のコンボ選択index
	int m_p2Select = 0;			// Player2 のコンボ選択index

	// 各プレイヤーの配置（向かい合わせ。値は実機で微調整）
	Vector3 m_p1Pos{ -150, 0, 0 };	// player1 は左
	Vector3 m_p2Pos{  150, 0, 0 };	// player2 は右
	float   m_p1FaceY = -PI / 2.0f;	// 相手(+X方向)を向く ※モデルの正面軸により要調整
	float   m_p2FaceY =  PI / 2.0f;	// 相手(-X方向)を向く ※同上

	// スプラインカメラ用の状態
	std::vector<Vector3> m_splinePoints;	// 制御点（ImGuiで編集）
	Vector3 m_splineLookat{ 0, 0, 0 };		// 固定注視点（ImGuiで編集）
	bool    m_splineActive = false;			// スプライン再生中か（キー/UIでトグル）
	bool    m_splineLoop   = true;			// 経路をループ（閉曲線）
	bool    m_showSplinePath = true;		// 経路を可視化
	float   m_splineT      = 0.0f;			// 進行パラメータ（0〜区間数）
	float   m_splineSpeed  = 0.5f;			// 進行速度（区間/秒）

	std::unique_ptr<Sphere> m_splineMarker;	// 現在地点を示す赤い球体
	std::unique_ptr<Sphere> m_lookatMarker;	// 注視点を示す青い球体
	std::unique_ptr<Sphere> m_pointMarker;	// 制御点を示す緑の球体

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

};

REGISTER_CLASS(CarScene)