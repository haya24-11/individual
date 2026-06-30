#pragma once
#include <memory>
#include <array>
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

	void debugModelSelect();

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
	std::string m_meshid{};

};

REGISTER_CLASS(CarScene)