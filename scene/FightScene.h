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

class FightScene : public IScene
{
	// 現在の姿勢を表すクォータニオン
	Quaternion m_RotationQ{};

	// 現在の姿勢を表す行列
	Matrix4x4 m_RotationMtx{};

	Matrix4x4 m_ScaleMtx{};
public:
	explicit FightScene();
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

	// 奥の壁（地面と同じ床タイル）。キャラの奥側（+Z）に立てる
	std::unique_ptr<CSprite> m_wall;
	float m_wallZ = 600.0f;		// 壁の奥行き位置（キャラは Z=0 に立っている）

	// カメラ（メイン: 格ゲー基本⇔スプライン。Free-flyはデバッグ用の別ビュー）
	enum class CamMode { Fighting, Spline };
	CamMode        m_camMode = CamMode::Fighting;	// 既定は基本カメラ
	FreeFlyCamera  m_freeCam;						// デバッグ第2ビューを操作する
	SplineCamera   m_splineCam;						// 曲線カメラ
	FightingCamera m_fightCam;						// 格ゲー基本カメラ

	// 手動フリー操作（メイン画面）
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
	int m_p2Select = 14;			// Player2 のコンボ選択index（P1 と同じ X Bot.fbx）
	float m_p1Scale = 1.0f;		// Player1(suzu/PMX) 専用スケール。MMDは約20単位なので約9倍でX Bot相当

	// ---- カメラワーク ライブラリ（ライブラリタブ）----
	void drawLibraryUI();			// タブの中身を描く
	void refreshLibraryLists();		// フォルダを走査して一覧キャッシュを更新

	// ファイル名（"X Bot.fbx" 等）からカタログを逆引きしてモデルを適用する。
	// 見つからなければ false（消したモデルのテイクを読んでも落ちないように）
	bool applyModelByName(const std::string& fileName, int& selectIndex, std::string& meshId);

	// 入力バッファ。日本語は UTF-8 で1文字3バイトなので余裕を取る
	char m_libChar[128] = "kiyo";
	char m_libMove[128] = "";
	char m_libTake[128] = "take1";

	// 一覧のキャッシュ。毎フレーム directory_iterator を回すのは無駄なので、
	// 選択が変わった時とボタンを押した時だけ更新する
	std::vector<std::string> m_libChars;
	std::vector<std::string> m_libMoves;
	std::vector<std::string> m_libTakes;

	std::string m_libStatus;		// 直前の操作結果（画面に表示する）


	// 各プレイヤーの配置（向かい合わせ。値は実機で微調整）
	Vector3 m_p1Pos{ -150, -100, 0 };	// player1 は左（原点が足元→地面Y=-100に接地）
	Vector3 m_p2Pos{  150, -100, 0 };	// player2 は右（原点が足元→地面Y=-100に接地）
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
	//std::unique_ptr<CAnimationMeshBlender> m_p1AnimMesh;	// X Bot スキンメッシュ
	//std::unique_ptr<CAnimationData>        m_uppercutData;	// モーションFBX保持
	//BoneCombMatrix m_p1BoneComb;		// ボーン行列定数バッstファ(b5)
	//float m_p1AnimFrame  = 0.0f;		// 再生フレーム
	//bool  m_p1Attacking  = false;		// 攻撃モーション再生中か
	//int   m_uppercutFrames = 0;			// モーションの総キー数
	//float m_animFps = 30.0f;			// 再生速度（キー/秒）

	std::unique_ptr<CAnimationMeshBlender>m_p1AnimMesh;
	CAnimationData m_motions;
	BoneCombMatrix m_p1BoneComb;

	struct MotionEntry { std::string name; std::string path; };
	std::vector<MotionEntry>m_motionList;
	int			m_motionSelect = 0;
	bool		m_motionSelectLoop = true;
	std::string m_p1IdleName  = "sword and shield idle";
	std::string m_p1AttackName ="sword and shield slash";

	aiAnimation* m_p1Current = nullptr;
	std::string m_p1CurrentName;
	int m_p1CurrentFrames = 0;
	bool m_p1Loop = true;
	float m_p1AnimFrame = 0.0f;
	float m_animFps = 30.0f;

	// 再生範囲と速度倍率。ステップのように「先頭だけを速く」再生するために使う
	int   m_p1FrameBegin = 0;		// 再生開始フレーム
	int   m_p1FrameEnd   = 0;		// 再生終了フレーム（ここで折り返す／終わる）
	float m_p1SpeedScale = 1.0f;	// このモーション固有の速度倍率

	// ステップの調整値。範囲が歩幅、倍率がキビキビ感になる
	// 前は walk（66フレームで1サイクル＝2歩／1フレーム約2.5進む）を素材にして脚の振りを小さく保つ。
	// run は1フレーム約7進むため、同じ長さでも歩幅が3倍近くなりダッシュに見える
	int   m_stepFwdRange[2]  = { 0, 16 };	// 前ステップ（walk）
	float m_stepFwdSpeed     = 2.5f;
	int   m_stepBackRange[2] = { 0, 16 };	// バックステップ（run (2)）
	float m_stepBackSpeed    = 2.0f;

	// プレイヤー2 アニメーション。P1 と同じ仕組みで構えをループ再生する
	// モーションの実体は m_motions（P1 と共有のキャッシュ）から借りる
	std::unique_ptr<CAnimationMeshBlender> m_p2AnimMesh;
	BoneCombMatrix m_p2BoneComb;
	aiAnimation*   m_p2Current = nullptr;
	std::string    m_p2IdleName = "sword and shield idle";
	int            m_p2CurrentFrames = 0;
	float          m_p2AnimFrame = 0.0f;

	bool playMotionP2(const std::string& name);

	// --- 当たり判定の可視化（形を出すだけ。判定そのものはまだ行わない）---
	bool m_showHitbox = true;		// 既定で表示（デバッグタブのチェックで消せる）
	std::unique_ptr<Box> m_hitBox;			// 描画用に使い回す。サイズは毎回設定する
	float m_hitboxAlpha = 1.0f;				// 線の濃さ

	// ボーン2点を囲む長方形を1つ描く（pad は厚みの半分、planeZ は戦う平面の奥行き）
	// 2D格闘として扱うため、箱の奥行きは持たせず planeZ 上に平らに置く
	void drawBoneBox(const Vector3& a, const Vector3& b, float pad, float planeZ, const Color& col);

	// やられ判定（緑）。world は描画に使うのと同じ行列、scale はその拡大率
	void drawHurtboxes(const CAnimationMeshBlender* mesh,
		const Matrix4x4& world, float scale, const Color& col);

	// 押し合い判定（青）。足元から頭まで1個。アニメには追従させない
	void drawPushbox(const CAnimationMeshBlender* mesh,
		const Matrix4x4& world, float scale, const Color& col);

	// ルートモーションによる移動。アニメが進んだぶんだけ m_p1Pos を動かすので足が滑らない
	bool  m_p1ApplyRootMotion = true;
	bool  m_p1RootValid  = false;	// 前フレームのルート位置を持っているか
	float m_p1RootPrevX  = 0.0f;
	float m_p1RootPrevZ  = 0.0f;

	void scanMotionFolder();
	aiAnimation* ensureMotionLoaded(const std::string& name);

	// beginFrame / endFrame を省くと全体を再生する（endFrame = -1 で末尾まで）
	bool playMotion(const std::string& name, bool loop,
		int beginFrame = 0, int endFrame = -1, float speedScale = 1.0f);
	static int countFrames(const aiAnimation* a);
	void drawMotionUI();

};

REGISTER_CLASS(FightScene)