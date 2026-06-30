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
	std::unique_ptr<CSprite> m_ground;					// 地面（板ポリ）

	// 今表示しているメッシュのID
	std::string m_meshid{};

};

REGISTER_CLASS(CarScene)