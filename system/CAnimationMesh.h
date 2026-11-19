#pragma once

#include "transform.h"
#include "CStaticMesh.h"
#include "AssimpPerse.h"
#include "CAnimationData.h"
#include "CTreeNode.h"	
#include "renderer.h"
#include "BoneCombMatrix.h"
#include "CStaticMeshRenderer.h"

class CAnimationMesh : public CStaticMesh
{
protected:
	// ボーン辞書
	std::unordered_map<std::string, BONE> m_BoneDictionary{};	// 20240714 DX化

	// カレントのアニメーションデータ
	aiAnimation* m_CurrentAnimation{};

	// assimp ノード名ツリー（親子関係がわかる）
	CTreeNode<std::string>	m_AssimpNodeNameTree{};

	// レンダラ
	CStaticMeshRenderer m_StaticMeshRenderer{};

	// ルート（Hips）の水平移動を無視するか。true でその場再生になる
	bool m_LockRootXZ = false;

	// 姿勢の上乗せ（モーション編集の FK 用）。ボーン名 → そのボーン自身の向きで足す回転。
	// nullptr なら何もしない（対戦画面ではいつも nullptr）
	const std::unordered_map<std::string, Quaternion>* m_PoseOffsets = nullptr;

	// ローカルポーズ生成
	void BuildLocalPoseMap(
		const aiAnimation* animationdata,
		int& CurrentFrame,
		std::unordered_map<std::string, SRTQ>& localposemap);

	// ローカルポーズに上乗せの回転をかける
	void ApplyPoseOffsets(std::unordered_map<std::string, SRTQ>& localposemap) const;

public:
	void SetCurentAnimation(aiAnimation* currentanimation);

	// ルート移動の水平成分を止める（歩き・走りをその場で再生したいとき）
	void SetLockRootXZ(bool lock) { m_LockRootXZ = lock; }

	// 指定フレームのルート（Hips）の水平位置をモデル空間で返す。
	// フレーム間の差分を取ることで、アニメの歩幅どおりにキャラを動かせる。
	bool GetRootXZ(int frame, float& outX, float& outZ) const;

	// ボーンのモデル空間での位置。当たり判定カプセルの芯に使う。
	// Update() を呼んだ後の姿勢を返すので、毎フレーム引き直すこと。
	bool GetBonePosition(const std::string& bonename, Vector3& out) const;

	// ボーンのモデル空間での行列（向き＋位置）。FK で関節を回すときの軸に使う
	bool GetBoneGlobalMatrix(const std::string& bonename, Matrix4x4& out) const;

	// 姿勢の上乗せを設定する。渡したマップは Update のたびに読むので、呼び出し側で持ち続けること
	void SetPoseOffsets(const std::unordered_map<std::string, Quaternion>* offsets) { m_PoseOffsets = offsets; }

	void Load(std::string filename, std::string texturedirectory = "");

	// 階層構造を考慮したボーンコンビネーション行列を更新
	void UpdateBoneMatrix(CTreeNode<std::string>* ptree, Matrix4x4 matrix);		// 20240714 DX化	

	// アニメーションの更新
	void Update(BoneCombMatrix& bonecombarray, int& CurrentFrame);

	// 描画
	void Draw();
};
