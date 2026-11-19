#pragma once
#include <string>
#include <vector>

class JsonValue;

// 攻撃判定1個。技の中で「何フレーム目から何フレーム目まで」「どこに」出るか
// フレームは格ゲーの数え方に合わせて 1 始まり（1F = 技を出した最初のフレーム）
struct HitboxKey
{
	int         begin = 1;		// 出始めのフレーム（含む）
	int         end   = 1;		// 消えるフレーム（含む）
	std::string bone;			// 追従するボーン名。空ならキャラの足元を基準にする
	float       offX  = 0.0f;	// 基準からのずれ。+X がキャラの前方（右向きのときの右）
	float       offY  = 0.0f;	// 基準からのずれ。+Y が上
	float       w     = 40.0f;	// 横幅
	float       h     = 40.0f;	// 高さ
};

// 姿勢のキー1個。素材の回転に「上乗せ」する回転を、技のフレームに置く
struct PoseKey
{
	int   frame = 1;						// 技のフレーム（1始まり）
	float q[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };	// 上乗せする回転（クォータニオン x, y, z, w）。ボーン自身の向きの中で回る
};

// 1本のボーンの姿勢キー。キーとキーの間はなめらかに補間し、
// 最初のキーより前・最後のキーより後は、そのキーの値のまま（アニメーションソフトと同じ）
struct BoneTrack
{
	std::string          bone;
	std::vector<PoseKey> keys;			// frame の小さい順
};

// 技1つぶんのデータ。FBX のどこを、どの速さで、何フレームかけて再生し、どこに判定を出すか
struct MoveData
{
	std::string name;				// 技名（ファイル名にもなる）
	std::string motion;				// 素材にする FBX の名前（拡張子なし）
	int         clipBegin = 0;		// 使うキーの範囲（FBX のキー番号）
	int         clipEnd   = -1;		// -1 なら末尾まで
	float       speed     = 1.0f;	// 再生速度の倍率
	std::vector<HitboxKey> hitboxes;

	// 姿勢の編集（アニメーションレイヤー）。素材の FBX には手を付けず、上から重ねる
	std::vector<BoneTrack> poseLayer;

	BoneTrack*       FindTrack(const std::string& bone);
	const BoneTrack* FindTrack(const std::string& bone) const;
	const PoseKey*   FindPoseKey(const std::string& bone, int frame) const;
	void SetPoseKey(const std::string& bone, int frame, const float q[4]);	// 同じフレームにあれば上書き
	bool RemovePoseKey(const std::string& bone, int frame);					// 空になったトラックも消す
	void ClearTrack(const std::string& bone);

	// 技の全体フレーム数（60fps 換算）。keysPerSec は FBX のキーが1秒に何個進むか
	int  TotalFrames(float keysPerSec) const;

	// 技のフレーム（1始まり）→ FBX のキー番号
	int  KeyAtFrame(int frame, float keysPerSec) const;

	// 攻撃判定から求めるフレームデータ。判定が無ければ false
	//  startup  … 発生（最初に判定が出るフレーム）
	//  active   … 持続（最初の判定から最後の判定までのフレーム数）
	//  recovery … 硬直（判定が消えてから技が終わるまでのフレーム数）
	bool FrameData(float keysPerSec, int& startup, int& active, int& recovery) const;

	// そのフレームに攻撃判定が出ているか
	bool IsActiveAt(int frame) const;

	JsonValue ToJson() const;
	bool      FromJson(const JsonValue& v);
};

// 技データの保存先（実行フォルダの MoveLibrary/<技名>.json）
namespace MoveLibrary
{
	std::vector<std::string> List();
	bool Save(const MoveData& move, std::string* error = nullptr);
	bool Load(const std::string& name, MoveData& out, std::string* error = nullptr);
	bool Remove(const std::string& name);
}
