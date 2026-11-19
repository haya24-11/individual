#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "../system/imgui/imgui.h"
#include "../system/commontypes.h"
#include "../system/SceneClassFactory.h"
#include "../system/IScene.h"
#include "../system/C3DShape.h"
#include "../system/Camera.h"
#include "../system/CSprite.h"
#include "../system/CAnimationMeshBlender.h"
#include "../system/CAnimationData.h"
#include "../system/BoneCombMatrix.h"
#include "MoveData.h"
#include "Hurtbox.h"

// モーション編集シーン。
// FBX の一部を切り出して「技」にし、コマ送りで確かめながら攻撃判定をフレーム単位で置く。
// 作った技は MoveLibrary/<技名>.json に保存する
class MotionScene : public IScene
{
public:
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void init() override;
	void dispose() override;

private:
	// ---- プレビュー用カメラ（キャラの周りを回る）----
	Camera  m_camera;
	Vector3 m_camTarget{ 0.0f, 95.0f, 0.0f };	// 見る点（キャラの腰あたり）
	float   m_camYaw   = 0.0f;					// 0 で -Z 側から見る（対戦画面と同じ向き）
	float   m_camPitch = 0.12f;					// 見下ろす角度（ラジアン）
	float   m_camDist  = 420.0f;				// 見る点からの距離
	void    updateCameraInput();				// マウス操作（ImGui のフレーム内で呼ぶ）
	void    applyCamera();						// 角度と距離からカメラの位置を決める
	void    setCameraView(float yaw, float pitch);

	// ---- モデルとモーション ----
	std::unique_ptr<CAnimationMeshBlender> m_mesh;
	BoneCombMatrix m_boneComb;
	CAnimationData m_motions;					// 読み込んだ FBX のキャッシュ

	struct MotionEntry { std::string name; std::string path; };
	std::vector<MotionEntry> m_motionList;		// assets/motion の FBX 一覧
	aiAnimation* m_anim     = nullptr;			// 今の素材モーション
	int          m_animKeys = 0;				// 素材の総キー数
	float        m_keysPerSec = 30.0f;			// 素材のキーが1秒に何個進むか（対戦画面と同じ 30）

	void scanMotionFolder();
	bool selectMotion(const std::string& name, bool resetClip);
	static int countKeys(const aiAnimation* a);

	// ---- 技データと再生 ----
	MoveData m_move;
	float m_frame    = 1.0f;		// 今のフレーム（1始まり。再生中は小数で進む）
	bool  m_playing  = false;
	bool  m_loop     = true;
	float m_playRate = 1.0f;		// スロー再生の倍率
	int   m_selHitbox = -1;			// 選んでいる攻撃判定（-1 で無し）

	int   m_repeatDir   = 0;		// ←→ を押しっぱなしにしたときの連続コマ送り
	float m_repeatTimer = 0.0f;

	int  totalFrames() const { return m_move.TotalFrames(m_keysPerSec); }
	int  currentFrame() const;
	void setFrame(int frame);		// 再生を止めてそのフレームへ
	void togglePlay();
	void handleKeys(float dt);
	void applyPose();				// 今のフレームの姿勢をメッシュとルート移動へ反映する

	// ---- 表示 ----
	bool m_showFloor    = true;
	bool m_showSkeleton = true;
	bool m_showHurtbox  = true;
	bool m_showHitbox   = true;
	bool m_applyRoot    = false;	// ルートモーションの移動量ぶんキャラを動かして見せる
	Vector3 m_rootOffset{};			// 今のフレームでのルート移動量（ワールド）

	std::unique_ptr<CSprite> m_floor;
	std::unique_ptr<Box>     m_box;	// 判定・関節の表示に使い回す

	Matrix4x4 worldMatrix() const;
	Vector3   hitboxCenter(const HitboxKey& h, const Matrix4x4& world) const;
	void drawFlatBox(const Vector3& center, float w, float h, const Color& col);
	void drawSkeleton(const Matrix4x4& world);
	void drawHurtboxes(const Matrix4x4& world);
	void drawHitboxes(const Matrix4x4& world);

	// ---- 姿勢の編集（FK）----
	// 3D 画面で関節をクリックして選び、出てくる回転リングをドラッグして回す。
	// 回した量は今のフレームの「姿勢キー」になり、技データ（m_move.poseLayer）に入る
	std::unordered_map<std::string, Quaternion> m_poseOffsets;	// 今のフレームの上乗せ回転（メッシュへ渡す）
	std::string m_poseBone;					// 選んでいる関節（空なら無し）
	bool  m_ringWorld  = true;				// リングの軸: true=ワールド（画面の上下左右）/ false=ボーン自身
	int   m_ringHover  = -1;				// マウスが乗っているリング（0=X 1=Y 2=Z）
	int   m_ringDrag   = -1;				// ドラッグ中のリング
	ImVec2 m_dragStartMouse{};				// ドラッグを始めたマウス位置（メイン画面の中の座標）
	ImVec2 m_dragTangent{};					// つかんだ所でのリングの接線（画面上の向き）
	float  m_dragRadiusPx = 1.0f;			// リングの画面上の半径
	Vector3    m_dragAxisModel{};			// 回す軸（モデル空間）
	Quaternion m_dragStartOffset{};			// ドラッグ開始時の上乗せ回転
	Quaternion m_dragBoneRot{};				// ドラッグ開始時のボーンの向き（モデル空間）

	// 元に戻す。姿勢キー・攻撃判定・やられ判定をまとめて控える
	struct UndoState
	{
		std::vector<BoneTrack>  pose;
		std::vector<HitboxKey>  hit;
		std::vector<HurtboxDef> hurt;
	};
	std::vector<UndoState> m_undo;

	Quaternion poseOffsetAt(const std::string& bone, int frame) const;	// キーを補間した上乗せ回転
	void  buildPoseOffsets();				// 今のフレームの上乗せ回転を全部のボーンぶん作る
	void  pushUndo();
	void  undo();
	float ringRadius() const { return m_camDist * 0.07f; }
	bool  ringAxisWorld(int axis, Vector3& outAxis) const;	// リングの軸（ワールド）
	bool  boneWorldPos(const std::string& bone, Vector3& out) const;
	bool  worldToScreen(const Vector3& p, ImVec2& out) const;	// メイン画面の中のピクセル座標へ
	void  ringBasis(const Vector3& axis, Vector3& u, Vector3& v) const;	// 軸に直交する2本（u→v が正の回転の向き）
	void  updatePoseInput();				// 関節の選択とリングのドラッグ（ImGui のフレーム内で呼ぶ）
	void  drawRings(const Matrix4x4& world);
	void  drawPoseUI();

	// ---- 3D 画面の左クリックで触るもの ----
	enum class EditMode { Pose, Hitbox, Hurtbox };
	EditMode m_editMode = EditMode::Pose;
	void setEditMode(EditMode mode);

	// ---- 判定の箱をドラッグで編集（攻撃判定・やられ判定 共通）----
	// 箱の中をつかむと移動、辺・角をつかむと大きさを変える。動かすのは戦う平面（XY）の上だけ
	enum class BoxKind { Hit, Hurt };
	int   m_selHurt   = -1;				// 選んでいるやられ判定（-1 で無し）
	bool  m_boxDrag   = false;			// ドラッグ中か
	BoxKind m_boxKind = BoxKind::Hit;	// ドラッグしている箱の種類
	int   m_boxIndex  = -1;				// ドラッグしている箱の番号
	int   m_boxSX = 0, m_boxSY = 0;		// つかんだ辺（-1=左/下 +1=右/上 0=動かさない）。両方 0 なら移動
	Vector3  m_boxStartPlane{};			// つかんだ瞬間のマウスの位置（平面上のワールド座標）
	HurtRect m_boxStartRect{};			// つかんだ瞬間の箱
	bool  m_hotValid = false;			// マウスが乗っているつまみ（表示用）
	int   m_hotSX = 0, m_hotSY = 0;

	bool screenToPlane(const ImVec2& mouse, float planeZ, Vector3& out) const;	// マウス → 平面上のワールド座標
	bool hitRect(int i, HurtRect& r) const;			// 攻撃判定 i の今の長方形
	bool hurtRect(int i, HurtRect& r) const;		// やられ判定 i の今の長方形
	bool selectedRect(HurtRect& r) const;			// 今のモードで選んでいる箱
	bool boxHandleAt(const HurtRect& r, const ImVec2& mouse, int& sx, int& sy, bool& inside) const;
	void applyBoxRect(const HurtRect& r);			// ドラッグ後の長方形を、判定のデータへ書き戻す
	void updateBoxInput();							// 箱の選択とドラッグ（ImGui のフレーム内で呼ぶ）
	void drawBoxHandles();
	void drawHurtUI();

	// ---- ツール ----
	char m_nameBuf[128] = "新しい技";
	std::vector<std::string> m_savedMoves;	// 保存済みの技の一覧
	int  m_savedSelect = -1;
	std::string m_status;					// 直前の操作の結果
	bool m_timelineScrub = false;			// タイムラインをドラッグ中か

	void refreshSavedList();
	void drawToolsUI();
	void drawFileUI();
	void drawSourceUI();
	void drawTransportUI();
	void drawTimelineUI();
	void drawHitboxUI();
	void drawViewUI();
};

REGISTER_CLASS(MotionScene)
