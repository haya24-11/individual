#include "MotionScene.h"
#include "../system/imgui/imgui.h"
#include "../system/DebugUI.h"
#include "../system/renderer.h"
#include "../system/CShader.h"
#include "../system/meshmanager.h"
#include "../system/CDirectInput.h"
#include "../system/LineDrawer.h"
#include "../system/scenemanager.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace
{
	// キャラは +X を向かせる（対戦画面の P1 と同じ向き）。判定の「前方」= +X になる
	constexpr float kFaceY = -PI / 2.0f;

	// 骨格表示でつなぐボーンの組（Mixamo の X Bot）
	struct BonePair { const char* from; const char* to; };
	const BonePair kSkeleton[] = {
		{ "mixamorig:Hips",          "mixamorig:Spine"         },
		{ "mixamorig:Spine",         "mixamorig:Spine1"        },
		{ "mixamorig:Spine1",        "mixamorig:Spine2"        },
		{ "mixamorig:Spine2",        "mixamorig:Neck"          },
		{ "mixamorig:Neck",          "mixamorig:Head"          },
		{ "mixamorig:Spine2",        "mixamorig:LeftShoulder"  },
		{ "mixamorig:LeftShoulder",  "mixamorig:LeftArm"       },
		{ "mixamorig:LeftArm",       "mixamorig:LeftForeArm"   },
		{ "mixamorig:LeftForeArm",   "mixamorig:LeftHand"      },
		{ "mixamorig:Spine2",        "mixamorig:RightShoulder" },
		{ "mixamorig:RightShoulder", "mixamorig:RightArm"      },
		{ "mixamorig:RightArm",      "mixamorig:RightForeArm"  },
		{ "mixamorig:RightForeArm",  "mixamorig:RightHand"     },
		{ "mixamorig:Hips",          "mixamorig:LeftUpLeg"     },
		{ "mixamorig:LeftUpLeg",     "mixamorig:LeftLeg"       },
		{ "mixamorig:LeftLeg",       "mixamorig:LeftFoot"      },
		{ "mixamorig:LeftFoot",      "mixamorig:LeftToeBase"   },
		{ "mixamorig:Hips",          "mixamorig:RightUpLeg"    },
		{ "mixamorig:RightUpLeg",    "mixamorig:RightLeg"      },
		{ "mixamorig:RightLeg",      "mixamorig:RightFoot"     },
		{ "mixamorig:RightFoot",     "mixamorig:RightToeBase"  },
	};

	// 攻撃判定を追従させられるボーン（UI のコンボに並べる）
	const char* kAttachBones[] = {
		"mixamorig:RightHand", "mixamorig:LeftHand",
		"mixamorig:RightForeArm", "mixamorig:LeftForeArm",
		"mixamorig:RightFoot", "mixamorig:LeftFoot",
		"mixamorig:RightLeg", "mixamorig:LeftLeg",
		"mixamorig:Head", "mixamorig:Spine2", "mixamorig:Hips",
	};

	// 表示用に "mixamorig:" を外す
	const char* ShortBoneName(const std::string& bone)
	{
		static const char kPrefix[] = "mixamorig:";
		if (bone.empty()) return "（足元）";
		if (bone.rfind(kPrefix, 0) == 0) return bone.c_str() + sizeof(kPrefix) - 1;
		return bone.c_str();
	}

	// 3D の線を1本描く（LineDrawer は「始点・向き・長さ」で受け取る）
	void DrawLine(const Vector3& a, const Vector3& b, const Color& col)
	{
		const Vector3 d = b - a;
		const float   len = d.Length();
		if (len < 1e-4f) return;
		LineDrawerDraw(len, a, d, col);
	}

	// FK で選べる関節（骨格表示に出ている骨すべて）。腰が先頭
	const std::vector<std::string>& PickBones()
	{
		static std::vector<std::string> list;
		if (list.empty())
		{
			for (const BonePair& p : kSkeleton)
				for (const char* b : { p.from, p.to })
					if (std::find(list.begin(), list.end(), b) == list.end()) list.push_back(b);
		}
		return list;
	}

	// クォータニオン → (X, Y, Z) の角度（度）。Quaternion::CreateFromYawPitchRoll(Y, X, Z) の逆
	Vector3 ToEulerDeg(const Quaternion& q)
	{
		const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
		const float m31 = 2.0f * q.x * q.z + 2.0f * q.y * q.w;
		const float m32 = 2.0f * q.y * q.z - 2.0f * q.x * q.w;
		const float m33 = 1.0f - 2.0f * xx - 2.0f * yy;
		const float cy  = sqrtf(m33 * m33 + m31 * m31);
		const float cx  = atan2f(-m32, cy);

		Vector3 r;
		if (cy > 16.0f * FLT_EPSILON)
			r = Vector3(cx, atan2f(m31, m33), atan2f(2.0f * q.x * q.y + 2.0f * q.z * q.w, 1.0f - 2.0f * xx - 2.0f * zz));
		else	// 真上・真下を向いたとき（ジンバルロック）は Y を 0 にまとめる
			r = Vector3(cx, 0.0f, atan2f(-(2.0f * q.x * q.y - 2.0f * q.z * q.w), 1.0f - 2.0f * yy - 2.0f * zz));
		return r * (180.0f / PI);
	}

	Quaternion FromEulerDeg(const Vector3& deg)
	{
		const Vector3 r = deg * (PI / 180.0f);
		return Quaternion::CreateFromYawPitchRoll(r.y, r.x, r.z);
	}

	// 回転リングの色（X 赤・Y 緑・Z 青、つかんでいる輪は黄色）
	const Color kRingCol[3] = { Color(1.0f, 0.25f, 0.25f, 1.0f), Color(0.3f, 1.0f, 0.3f, 1.0f), Color(0.35f, 0.55f, 1.0f, 1.0f) };
	const Color kRingHot(1.0f, 0.95f, 0.2f, 1.0f);
	constexpr int kRingSegments = 64;

	// 点 p から線分 ab までの距離（画面のピクセル）
	float DistToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
	{
		const float abx = b.x - a.x, aby = b.y - a.y;
		const float len2 = abx * abx + aby * aby;
		float t = (len2 > 1e-6f) ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2 : 0.0f;
		t = std::clamp(t, 0.0f, 1.0f);
		const float dx = a.x + abx * t - p.x, dy = a.y + aby * t - p.y;
		return sqrtf(dx * dx + dy * dy);
	}

	// タイムラインの色
	constexpr ImU32 kColStartup  = IM_COL32( 60, 170,  90, 255);	// 発生前（緑）
	constexpr ImU32 kColActive   = IM_COL32(220,  60,  60, 255);	// 判定が出ている（赤）
	constexpr ImU32 kColGap      = IM_COL32(120,  50,  50, 255);	// 持続の中の隙間（暗い赤）
	constexpr ImU32 kColRecovery = IM_COL32( 60, 110, 210, 255);	// 硬直（青）
	constexpr ImU32 kColNone     = IM_COL32( 90,  90,  90, 255);	// 判定なし（灰）
}

// ================================================================ 初期化

void MotionScene::init()
{
	// カメラ
	m_camera.Init();
	m_camera.SetUP(Vector3(0, 1, 0));
	applyCamera();

	// シェーダ。対戦画面が先に登録していればそれを使う（同じ名前で二重に作らない）
	if (!ShaderManager::Contains("Shader3D"))
	{
		auto s = std::make_unique<CShader>();
		s->Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");
		ShaderManager::Register<CShader>("Shader3D", std::move(s));
	}
	if (!ShaderManager::Contains("Skin3D"))
	{
		auto s = std::make_unique<CShader>();
		s->Create("shader/vertexLightingOneSkinVS.hlsl", "shader/vertexLightingPS.hlsl");
		ShaderManager::Register<CShader>("Skin3D", std::move(s));
	}

	// X Bot（スキン+ボーン）。ルートの水平移動はその場再生にして、移動量は別に計算する
	m_mesh = std::make_unique<CAnimationMeshBlender>();
	m_mesh->Load("assets/motion/X Bot.fbx", "assets/motion/");
	m_mesh->SetLockRootXZ(true);
	m_mesh->SetPoseOffsets(&m_poseOffsets);	// FK で足した回転を毎回かけてもらう
	m_boneComb.Create();

	// 床（対戦画面と同じタイル。原点が足元なので Y=0 に置く）
	const float tile = 10.0f;
	std::array<Vector2, 4> uv = { Vector2(0, 0), Vector2(tile, 0), Vector2(0, tile), Vector2(tile, tile) };
	m_floor = std::make_unique<CSprite>(2000, 2000, "assets/texture/floortile.png", uv);

	m_box = std::make_unique<Box>(10.0f, 10.0f, 10.0f);

	// 素材の一覧を作り、最初の技の素材を決める
	scanMotionFolder();
	if (!selectMotion("sword and shield slash", true) && !m_motionList.empty())
		selectMotion(m_motionList.front().name, true);

	refreshSavedList();
	setFrame(1);

	// ツール窓。this を渡すので、このシーンが表示中のときだけ出る
	DebugUI::RedistDebugFunction([this]() { drawToolsUI(); }, this);
}

void MotionScene::dispose()
{
}

void MotionScene::scanMotionFolder()
{
	namespace fs = std::filesystem;
	m_motionList.clear();

	const fs::path root = "assets/motion";
	if (!fs::exists(root)) return;

	for (const auto& e : fs::recursive_directory_iterator(root))
	{
		if (!e.is_regular_file() || e.path().extension() != ".fbx") continue;
		std::string name = e.path().stem().string();
		if (name == "X Bot") continue;	// モデル本体はモーションではない
		m_motionList.push_back({ name, e.path().generic_string() });
	}
	std::sort(m_motionList.begin(), m_motionList.end(),
		[](const MotionEntry& a, const MotionEntry& b) { return a.name < b.name; });
}

int MotionScene::countKeys(const aiAnimation* a)
{
	int n = 0;
	for (unsigned int c = 0; c < a->mNumChannels; ++c)
		n = std::max(n, (int)a->mChannels[c]->mNumRotationKeys);
	return n;
}

// 素材のモーションを切り替える。resetClip なら使う範囲を全体に戻す
bool MotionScene::selectMotion(const std::string& name, bool resetClip)
{
	auto it = std::find_if(m_motionList.begin(), m_motionList.end(),
		[&](const MotionEntry& m) { return m.name == name; });
	if (it == m_motionList.end()) return false;

	if (!m_motions.Contains(name) && !m_motions.LoadAnimation(it->path, name)) return false;
	aiAnimation* a = m_motions.GetAnimation(name);
	if (!a || !m_mesh) return false;

	m_anim     = a;
	m_animKeys = countKeys(a);
	m_mesh->SetCurentAnimation(a);

	const int last = std::max(m_animKeys - 1, 0);
	m_move.motion = name;
	if (resetClip)
	{
		m_move.clipBegin = 0;
		m_move.clipEnd   = last;
	}
	else
	{
		m_move.clipBegin = std::clamp(m_move.clipBegin, 0, last);
		m_move.clipEnd   = (m_move.clipEnd < 0) ? last : std::clamp(m_move.clipEnd, m_move.clipBegin, last);
	}
	setFrame(currentFrame());
	return true;
}

// ================================================================ 再生

int MotionScene::currentFrame() const
{
	return std::clamp((int)m_frame, 1, totalFrames());
}

void MotionScene::setFrame(int frame)
{
	m_playing = false;
	m_frame   = (float)std::clamp(frame, 1, totalFrames());
}

void MotionScene::togglePlay()
{
	// 最後まで行って止まっているときは、先頭から流し直す
	if (!m_playing && currentFrame() >= totalFrames()) m_frame = 1.0f;
	m_playing = !m_playing;
}

// Space で再生/停止、←→ でコマ送り（押しっぱなしで連続）
void MotionScene::handleKeys(float dt)
{
	if (ImGui::GetIO().WantTextInput) return;	// 技名の入力中はキーを奪わない

	CDirectInput& in = CDirectInput::GetInstance();
	if (in.CheckKeyBufferTrigger(DIK_SPACE)) togglePlay();

	int dir = 0;
	if      (in.CheckKeyBuffer(DIK_LEFT))  dir = -1;
	else if (in.CheckKeyBuffer(DIK_RIGHT)) dir =  1;

	if (dir == 0)
	{
		m_repeatDir = 0;
		return;
	}
	if (dir != m_repeatDir)					// 押した瞬間に1コマ
	{
		setFrame(currentFrame() + dir);
		m_repeatDir   = dir;
		m_repeatTimer = 0.35f;				// 少し待ってから連続にする
		return;
	}
	m_repeatTimer -= dt;
	if (m_repeatTimer <= 0.0f)
	{
		setFrame(currentFrame() + dir);
		m_repeatTimer = 0.05f;
	}
}

void MotionScene::applyPose()
{
	if (!m_mesh || !m_anim) return;

	int key = m_move.KeyAtFrame(currentFrame(), m_keysPerSec);

	// ルートモーション：技の先頭からどれだけ進んだかを、描画と同じ向きでワールドへ移す
	m_rootOffset = Vector3::Zero;
	if (m_applyRoot)
	{
		float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
		if (m_mesh->GetRootXZ(m_move.clipBegin, x0, z0) && m_mesh->GetRootXZ(key, x1, z1))
		{
			const Vector3 local(x1 - x0, 0.0f, z1 - z0);
			m_rootOffset = Vector3::TransformNormal(local, Matrix4x4::CreateRotationY(kFaceY));
		}
	}

	buildPoseOffsets();
	m_mesh->Update(m_boneComb, key);
}

void MotionScene::update(uint64_t deltatime)
{
	const float dt = deltatime / 1'000'000.0f;

	handleKeys(dt);

	if (m_playing && m_anim)
	{
		// 60fps のゲームフレームで進める。最終フレームも1フレームぶん表示してから折り返す
		m_frame += 60.0f * m_playRate * dt;
		const int total = totalFrames();
		if (m_frame >= total + 1.0f)
		{
			if (m_loop) m_frame = 1.0f;
			else { m_frame = (float)total; m_playing = false; }
		}
	}

	applyPose();
}

// ================================================================ カメラ

void MotionScene::applyCamera()
{
	const float cp = cosf(m_camPitch);
	const Vector3 dir(sinf(m_camYaw) * cp, sinf(m_camPitch), -cosf(m_camYaw) * cp);
	m_camera.SetPosition(m_camTarget + dir * m_camDist);
	m_camera.SetLookat(m_camTarget);
}

void MotionScene::setCameraView(float yaw, float pitch)
{
	m_camYaw   = yaw;
	m_camPitch = pitch;
	applyCamera();
}

// 右ドラッグで回転、中ドラッグで上下移動、ホイールで寄る／引く。
// ImGui の窓の上では効かせない（スライダー操作と取り合わないように）
void MotionScene::updateCameraInput()
{
	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) return;

	if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
	{
		m_camYaw  -= io.MouseDelta.x * 0.01f;
		m_camPitch = std::clamp(m_camPitch + io.MouseDelta.y * 0.01f, -1.4f, 1.4f);
	}
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		m_camTarget.y += io.MouseDelta.y * m_camDist * 0.002f;
	}
	if (io.MouseWheel != 0.0f)
	{
		m_camDist = std::clamp(m_camDist * powf(0.9f, io.MouseWheel), 80.0f, 2000.0f);
	}
	applyCamera();
}

// ================================================================ 描画

Matrix4x4 MotionScene::worldMatrix() const
{
	return Matrix4x4::CreateRotationY(kFaceY) * Matrix4x4::CreateTranslation(m_rootOffset);
}

// 攻撃判定の中心。ボーンに付けていればその関節、無ければ足元からのずれ
Vector3 MotionScene::hitboxCenter(const HitboxKey& h, const Matrix4x4& world) const
{
	Vector3 base = world.Translation();
	Vector3 bonePos;
	if (!h.bone.empty() && m_mesh && m_mesh->GetBonePosition(h.bone, bonePos))
		base = Vector3::Transform(bonePos, world);
	return Vector3(base.x + h.offX, base.y + h.offY, world.Translation().z);
}

// 戦う平面（XY）に平らな箱を置く。対戦画面の判定表示と同じ考え方
void MotionScene::drawFlatBox(const Vector3& center, float w, float h, const Color& col)
{
	m_box->SetSize(std::max(w, 0.1f), std::max(h, 0.1f), 2.0f);
	m_box->Draw(Matrix4x4::CreateTranslation(center), col);
}

void MotionScene::drawSkeleton(const Matrix4x4& world)
{
	const Color lineCol(1.0f, 0.85f, 0.2f, 1.0f);
	const Color jointCol(1.0f, 1.0f, 1.0f, 1.0f);

	for (const BonePair& p : kSkeleton)
	{
		Vector3 a, b;
		if (!m_mesh->GetBonePosition(p.from, a) || !m_mesh->GetBonePosition(p.to, b)) continue;
		DrawLine(Vector3::Transform(a, world), Vector3::Transform(b, world), lineCol);
	}

	// 関節は小さな箱。FK で選んでいる関節は水色、編集した関節は橙、判定を付けているボーンは黄色で大きく見せる
	const std::string selBone = (m_selHitbox >= 0) ? m_move.hitboxes[m_selHitbox].bone : std::string();
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	for (const std::string& name : PickBones())
	{
		Vector3 b;
		if (!m_mesh->GetBonePosition(name, b)) continue;

		float s   = 3.0f;
		Color col = jointCol;
		if (m_move.FindTrack(name))               { s = 4.5f; col = Color(1.0f, 0.55f, 0.15f, 1.0f); }
		if (!selBone.empty() && selBone == name)  { s = 8.0f; col = Color(1.0f, 1.0f, 0.0f, 1.0f); }
		if (name == m_poseBone)                   { s = 7.0f; col = Color(0.2f, 0.9f, 1.0f, 1.0f); }

		m_box->SetSize(s, s, s);
		m_box->Draw(Matrix4x4::CreateTranslation(Vector3::Transform(b, world)), col);
	}
}

void MotionScene::drawHurtboxes(const Matrix4x4& world)
{
	// 形は対戦画面と共通（HurtboxConfig.json）。選んでいる箱は水色
	const Color green(0.1f, 1.0f, 0.1f, 1.0f);
	const Color selCol(0.2f, 0.9f, 1.0f, 1.0f);
	const int n = (int)Hurtbox::Defs().size();
	for (int i = 0; i < n; ++i)
	{
		HurtRect r;
		if (!hurtRect(i, r)) continue;
		drawFlatBox(Vector3(r.cx(), r.cy(), world.Translation().z), r.w(), r.h(), (i == m_selHurt) ? selCol : green);
	}
}

// 今のフレームに出ている攻撃判定を赤で描く。選んでいる判定は、出ていないフレームでも暗い黄色で見せる
void MotionScene::drawHitboxes(const Matrix4x4& world)
{
	const int f = currentFrame();
	for (int i = 0; i < (int)m_move.hitboxes.size(); ++i)
	{
		const HitboxKey& h = m_move.hitboxes[i];
		const bool active = (f >= h.begin && f <= h.end);
		const bool sel    = (i == m_selHitbox);
		if (!active && !sel) continue;

		Color col = active ? Color(1.0f, 0.15f, 0.15f, 1.0f) : Color(0.5f, 0.45f, 0.1f, 1.0f);
		if (active && sel) col = Color(1.0f, 0.6f, 0.1f, 1.0f);	// 選択中で出ている＝橙
		drawFlatBox(hitboxCenter(h, world), h.w, h.h, col);
	}
}

void MotionScene::draw(uint64_t deltatime)
{
	m_camera.Draw();

	// 床
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	if (m_showFloor && m_floor)
	{
		Renderer::DisableCulling(false);
		m_floor->Draw(Vector3(1, 1, 1), Vector3(PI / 2.0f, 0, 0), Vector3(0, 0, 0));
		Renderer::DisableCulling(true);
	}

	// キャラ
	const Matrix4x4 world = worldMatrix();
	if (m_mesh && m_anim)
	{
		Renderer::SetWorldMatrix(const_cast<Matrix4x4*>(&world));
		ShaderManager::Get<CShader>("Skin3D")->SetGPU();
		m_boneComb.Update();
		m_boneComb.SetGPU();
		m_mesh->Draw();
	}
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();

	if (!m_mesh || !m_anim) return;

	// ここから先の可視化は、奥行きを無視して常に手前に出す
	Renderer::SetDepthEnable(false);

	// 線の太さはジオメトリシェーダの定数バッファで渡す。対戦画面に頼らず、ここで毎回決める
	// （設定しないままだと太さ 0 になり、線が見えない）
	SetLineWidth(2.0f);

	// 技を出した位置の目印（床の上の短い十字）。ルートモーションでどれだけ進んだかがわかる
	DrawLine(Vector3(-30, 0.5f, 0), Vector3(30, 0.5f, 0), Color(1, 1, 1, 1));
	DrawLine(Vector3(0, 0.5f, -30), Vector3(0, 0.5f, 30), Color(1, 1, 1, 1));

	if (m_showSkeleton) drawSkeleton(world);
	if (m_editMode == EditMode::Pose)
	{
		SetLineWidth(3.0f);		// 回転リングは少し太く
		drawRings(world);
		SetLineWidth(2.0f);
	}

	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	Renderer::SetFillMode(D3D11_FILL_WIREFRAME);
	if (m_showHurtbox) drawHurtboxes(world);
	if (m_showHitbox)  drawHitboxes(world);
	Renderer::SetFillMode(D3D11_FILL_SOLID);

	drawBoxHandles();		// 選んでいる箱の辺・角のつまみ

	Renderer::SetDepthEnable(true);
}

// ================================================================ ツール

void MotionScene::refreshSavedList()
{
	m_savedMoves = MoveLibrary::List();
	if (m_savedSelect >= (int)m_savedMoves.size()) m_savedSelect = (int)m_savedMoves.size() - 1;
}

void MotionScene::drawToolsUI()
{
	updateCameraInput();	// マウスの状態は ImGui のフレームの中で読むのが確実
	// 左クリックで触るものによって、関節（FK）か判定の箱かを扱う
	if (m_editMode == EditMode::Pose) updatePoseInput();
	else                              updateBoxInput();

	// Ctrl+Z で元に戻す、Esc で選択を外す、1/2/3 で触るものを切り替える（文字の入力中は奪わない）
	const ImGuiIO& io = ImGui::GetIO();
	if (!io.WantTextInput)
	{
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo();
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_ringDrag < 0 && !m_boxDrag)
		{
			if (m_editMode == EditMode::Pose)    m_poseBone.clear();
			if (m_editMode == EditMode::Hitbox)  m_selHitbox = -1;
			if (m_editMode == EditMode::Hurtbox) m_selHurt = -1;
		}
		if (!io.KeyCtrl)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_1, false)) setEditMode(EditMode::Pose);
			if (ImGui::IsKeyPressed(ImGuiKey_2, false)) setEditMode(EditMode::Hitbox);
			if (ImGui::IsKeyPressed(ImGuiKey_3, false)) setEditMode(EditMode::Hurtbox);
		}
	}

	ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560, 760), ImGuiCond_FirstUseEver);
	ImGui::Begin("モーション編集 ツール###Motion Tools");

	if (ImGui::Button("← 対戦画面へ戻る##to fight")) SceneManager::RequestScene("FightScene");
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("Space 再生/停止   ←→ コマ送り   右ドラッグ 視点   中ドラッグ 上下   ホイール 寄る/引く   Ctrl+Z 元に戻す");
	ImGui::PopStyleColor();

	// 3D 画面の左クリックで触るもの
	ImGui::TextUnformatted("左クリックで触る:");
	ImGui::SameLine();
	if (ImGui::RadioButton("関節 (1)##mode pose",      m_editMode == EditMode::Pose))    setEditMode(EditMode::Pose);
	ImGui::SameLine();
	if (ImGui::RadioButton("攻撃判定 (2)##mode hit",   m_editMode == EditMode::Hitbox))  setEditMode(EditMode::Hitbox);
	ImGui::SameLine();
	if (ImGui::RadioButton("やられ判定 (3)##mode hurt", m_editMode == EditMode::Hurtbox)) setEditMode(EditMode::Hurtbox);

	if (ImGui::CollapsingHeader("技ファイル##file", ImGuiTreeNodeFlags_DefaultOpen))           drawFileUI();
	if (ImGui::CollapsingHeader("素材モーション##source", ImGuiTreeNodeFlags_DefaultOpen))      drawSourceUI();
	if (ImGui::CollapsingHeader("再生とタイムライン##timeline", ImGuiTreeNodeFlags_DefaultOpen))
	{
		drawTransportUI();
		drawTimelineUI();
	}
	if (ImGui::CollapsingHeader("姿勢の編集（FK）##pose", ImGuiTreeNodeFlags_DefaultOpen))      drawPoseUI();
	if (ImGui::CollapsingHeader("攻撃判定##hitbox", ImGuiTreeNodeFlags_DefaultOpen))           drawHitboxUI();
	if (ImGui::CollapsingHeader("やられ判定（キャラ共通）##hurtbox"))                           drawHurtUI();
	if (ImGui::CollapsingHeader("表示##view"))                                                 drawViewUI();

	ImGui::End();
}

void MotionScene::drawFileUI()
{
	ImGui::InputText("技名##move name", m_nameBuf, sizeof(m_nameBuf));

	if (ImGui::Button("保存##save move"))
	{
		m_move.name = m_nameBuf;
		std::string err;
		if (MoveLibrary::Save(m_move, &err))
		{
			m_status = "保存しました: MoveLibrary/" + m_move.name + ".json";
			refreshSavedList();
			auto it = std::find(m_savedMoves.begin(), m_savedMoves.end(), m_move.name);
			if (it != m_savedMoves.end()) m_savedSelect = (int)(it - m_savedMoves.begin());
		}
		else m_status = "保存できません: " + err;
	}
	ImGui::SameLine();
	if (ImGui::Button("新規##new move"))
	{
		m_move.hitboxes.clear();
		m_move.poseLayer.clear();
		m_undo.clear();
		m_selHitbox = -1;
		selectMotion(m_move.motion, true);
		strncpy_s(m_nameBuf, "新しい技", _TRUNCATE);
		m_status = "新しい技を作ります（素材は今のモーション）";
	}

	// 保存済みの技
	const char* preview = (m_savedSelect >= 0 && m_savedSelect < (int)m_savedMoves.size())
		? m_savedMoves[m_savedSelect].c_str() : "（選んでください）";
	if (ImGui::BeginCombo("保存済み##saved moves", preview))
	{
		for (int i = 0; i < (int)m_savedMoves.size(); ++i)
			if (ImGui::Selectable(m_savedMoves[i].c_str(), i == m_savedSelect)) m_savedSelect = i;
		ImGui::EndCombo();
	}

	const bool hasSel = (m_savedSelect >= 0 && m_savedSelect < (int)m_savedMoves.size());
	ImGui::BeginDisabled(!hasSel);
	if (ImGui::Button("読み込む##load move"))
	{
		MoveData loaded;
		std::string err;
		if (MoveLibrary::Load(m_savedMoves[m_savedSelect], loaded, &err))
		{
			m_move = loaded;
			m_undo.clear();
			strncpy_s(m_nameBuf, m_move.name.c_str(), _TRUNCATE);
			m_selHitbox = m_move.hitboxes.empty() ? -1 : 0;
			if (selectMotion(m_move.motion, false)) m_status = "読み込みました: " + m_move.name;
			else m_status = "素材のモーションが見つかりません: " + m_move.motion;
			setFrame(1);
		}
		else m_status = "読み込めません: " + err;
	}
	ImGui::SameLine();
	if (ImGui::Button("削除…##delete move")) ImGui::OpenPopup("技の削除##confirm delete");
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("一覧を更新##refresh moves")) refreshSavedList();

	if (ImGui::BeginPopupModal("技の削除##confirm delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (hasSel)
		{
			ImGui::Text("「%s」を削除します。元に戻せません。", m_savedMoves[m_savedSelect].c_str());
			if (ImGui::Button("削除する##do delete"))
			{
				const std::string name = m_savedMoves[m_savedSelect];
				m_status = MoveLibrary::Remove(name) ? "削除しました: " + name : "削除できません: " + name;
				refreshSavedList();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (ImGui::Button("やめる##cancel delete")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
}

void MotionScene::drawSourceUI()
{
	if (m_motionList.empty())
	{
		ImGui::TextDisabled("assets/motion に FBX がありません");
		return;
	}

	if (ImGui::BeginCombo("モーション##source motion", m_move.motion.c_str()))
	{
		for (const MotionEntry& m : m_motionList)
		{
			const bool sel = (m.name == m_move.motion);
			if (ImGui::Selectable(m.name.c_str(), sel) && !sel) selectMotion(m.name, true);
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const int last = std::max(m_animKeys - 1, 0);
	if (ImGui::DragIntRange2("使う範囲（キー）##clip", &m_move.clipBegin, &m_move.clipEnd, 0.2f, 0, last,
		"開始 %d", "終了 %d"))
	{
		m_move.clipBegin = std::clamp(m_move.clipBegin, 0, last);
		m_move.clipEnd   = std::clamp(m_move.clipEnd, m_move.clipBegin, last);
		setFrame(currentFrame());
	}
	if (ImGui::SliderFloat("速度##move speed", &m_move.speed, 0.25f, 4.0f, "%.2f 倍"))
		setFrame(currentFrame());
	ImGui::SliderFloat("素材のキー/秒##keys per sec", &m_keysPerSec, 10.0f, 60.0f, "%.0f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("対戦画面の「再生速度（キー/秒）」と同じ値にすると、\nここで数えたフレームが対戦画面と一致します（既定 30）");

	const int total = totalFrames();
	ImGui::Text("素材 全%dキー → 使う %dキー → 技の全体 %dF（%.2f 秒）",
		m_animKeys, m_move.clipEnd - m_move.clipBegin + 1, total, total / 60.0f);
}

void MotionScene::drawTransportUI()
{
	const int total = totalFrames();
	const int cur   = currentFrame();

	if (ImGui::Button("|< 先頭##first")) setFrame(1);
	ImGui::SameLine();
	if (ImGui::Button("< 前##prev")) setFrame(cur - 1);
	ImGui::SameLine();
	if (ImGui::Button(m_playing ? "停止##play" : "再生##play", ImVec2(64, 0))) togglePlay();
	ImGui::SameLine();
	if (ImGui::Button("次 >##next")) setFrame(cur + 1);
	ImGui::SameLine();
	if (ImGui::Button("末尾 >|##last")) setFrame(total);
	ImGui::SameLine();
	ImGui::Checkbox("ループ##loop", &m_loop);

	static const float kRates[] = { 1.0f, 0.5f, 0.25f, 0.1f };
	static const char* kRateNames[] = { "等速", "1/2", "1/4", "1/10" };
	int rateIdx = 0;
	for (int i = 0; i < 4; ++i) if (fabsf(m_playRate - kRates[i]) < 1e-3f) rateIdx = i;
	ImGui::SetNextItemWidth(120);
	if (ImGui::Combo("スロー##rate", &rateIdx, kRateNames, 4)) m_playRate = kRates[rateIdx];

	int f = cur;
	if (ImGui::SliderInt("フレーム##frame", &f, 1, total, "%dF")) setFrame(f);

	ImGui::Text("%d / %d F    素材のキー %d", cur, total, m_move.KeyAtFrame(cur, m_keysPerSec));
}

// フレームを1マスずつ並べた帯。色で 発生前（緑）／判定（赤）／硬直（青）がひと目でわかる。
// 帯をクリック・ドラッグでそのフレームへ、下の行をクリックでその攻撃判定を選ぶ
void MotionScene::drawTimelineUI()
{
	const int total = totalFrames();
	const int cur   = currentFrame();

	int startup = 0, active = 0, recovery = 0;
	const bool hasData = m_move.FrameData(m_keysPerSec, startup, active, recovery);
	const int  lastActive = startup + active - 1;

	const float width  = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
	const float cellW  = width / total;
	const float barH   = 24.0f;
	const float rulerH = 16.0f;
	const float rowH   = 12.0f;
	const int   rows   = (int)m_move.hitboxes.size();
	const float height = barH + rulerH + rows * rowH + 4.0f;

	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##timeline", ImVec2(width, height));
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// マウス操作
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const int    mouseFrame = std::clamp(1 + (int)((mouse.x - p0.x) / cellW), 1, total);
	if (ImGui::IsItemActivated())
	{
		const float rowY = mouse.y - (p0.y + barH + rulerH);
		const int   row  = (rowY >= 0.0f) ? (int)(rowY / rowH) : -1;
		m_timelineScrub = !(row >= 0 && row < rows);
		if (!m_timelineScrub) m_selHitbox = row;
		setFrame(mouseFrame);
	}
	if (ImGui::IsItemActive() && m_timelineScrub) setFrame(mouseFrame);
	if (!ImGui::IsItemActive()) m_timelineScrub = false;

	// フレームの帯
	for (int f = 1; f <= total; ++f)
	{
		ImU32 col = kColNone;
		if (hasData)
		{
			if      (f < startup)          col = kColStartup;
			else if (f > lastActive)       col = kColRecovery;
			else if (m_move.IsActiveAt(f)) col = kColActive;
			else                           col = kColGap;
		}
		const float x0 = p0.x + (f - 1) * cellW;
		dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x0 + cellW, p0.y + barH), col);
		if (cellW >= 4.0f)	// マスが見分けられる幅のときだけ区切り線
			dl->AddLine(ImVec2(x0, p0.y), ImVec2(x0, p0.y + barH), IM_COL32(0, 0, 0, 120));
	}

	// 目盛り（10F ごと）
	// 数字どうしが重ならない間隔（最低 36px）を 5,10,20,50,100… から選ぶ
	int step = 5;
	for (int s : { 5, 10, 20, 50, 100, 200, 500 })
	{
		step = s;
		if (s * cellW >= 36.0f) break;
	}
	for (int f = 1; f <= total; ++f)
	{
		if (f != 1 && f % step != 0) continue;
		if (f == 1 && step * cellW < 48.0f && total >= step) continue;	// 1 と最初の目盛りが近すぎるときは 1 を省く
		const float x = p0.x + (f - 0.5f) * cellW;
		dl->AddLine(ImVec2(x, p0.y + barH), ImVec2(x, p0.y + barH + 4), IM_COL32(200, 200, 200, 255));
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", f);
		dl->AddText(ImVec2(x - 4, p0.y + barH + 2), IM_COL32(200, 200, 200, 255), buf);
	}

	// 攻撃判定ごとの行
	for (int i = 0; i < rows; ++i)
	{
		const HitboxKey& h = m_move.hitboxes[i];
		const float y0 = p0.y + barH + rulerH + i * rowH;
		const float x0 = p0.x + (std::clamp(h.begin, 1, total) - 1) * cellW;
		const float x1 = p0.x + std::clamp(h.end, 1, total) * cellW;
		const ImU32 col = (i == m_selHitbox) ? IM_COL32(255, 200, 40, 255) : IM_COL32(200, 70, 70, 255);
		dl->AddRectFilled(ImVec2(x0, y0 + 1), ImVec2(std::max(x1, x0 + 2), y0 + rowH - 1), col, 2.0f);
	}

	// 姿勢キー。選んでいる関節のキーは黄色のひし形、ほかの関節のキーは帯の上の小さな白い点
	for (const BoneTrack& t : m_move.poseLayer)
	{
		if (t.bone == m_poseBone) continue;
		for (const PoseKey& k : t.keys)
		{
			if (k.frame < 1 || k.frame > total) continue;
			const float x = p0.x + (k.frame - 0.5f) * cellW;
			dl->AddCircleFilled(ImVec2(x, p0.y + 4.0f), 2.0f, IM_COL32(255, 255, 255, 200));
		}
	}
	if (const BoneTrack* t = m_move.FindTrack(m_poseBone))
	{
		for (const PoseKey& k : t->keys)
		{
			if (k.frame < 1 || k.frame > total) continue;
			const float x = p0.x + (k.frame - 0.5f) * cellW, y = p0.y + barH - 7.0f;
			const float r = 5.0f;
			dl->AddQuadFilled(ImVec2(x, y - r), ImVec2(x + r, y), ImVec2(x, y + r), ImVec2(x - r, y), IM_COL32(255, 220, 40, 255));
			dl->AddQuad(ImVec2(x, y - r), ImVec2(x + r, y), ImVec2(x, y + r), ImVec2(x - r, y), IM_COL32(0, 0, 0, 200));
		}
	}

	// 今のフレーム
	{
		const float x0 = p0.x + (cur - 1) * cellW;
		dl->AddRect(ImVec2(x0, p0.y - 2), ImVec2(x0 + std::max(cellW, 2.0f), p0.y + height),
			IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
	}

	// フレームデータ
	if (hasData)
		ImGui::Text("発生 %dF   持続 %dF   硬直 %dF   全体 %dF", startup, active, recovery, total);
	else
		ImGui::TextDisabled("攻撃判定がありません（全体 %dF）", total);

	ImGui::TextColored(ImVec4(0.24f, 0.67f, 0.35f, 1), "■発生前");  ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.86f, 0.24f, 0.24f, 1), "■判定");    ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.47f, 0.20f, 0.20f, 1), "■隙間");    ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.24f, 0.43f, 0.82f, 1), "■硬直");     ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.00f, 0.86f, 0.16f, 1), "◆姿勢キー");
}

void MotionScene::drawHitboxUI()
{
	const int total = totalFrames();
	const int cur   = currentFrame();

	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("「攻撃判定 (2)」にすると、3D 画面で箱の中をドラッグして移動、辺・角をドラッグして大きさを変えられます");
	ImGui::PopStyleColor();

	if (ImGui::Button("＋ 今のフレームに追加##add hitbox"))
	{
		pushUndo();
		HitboxKey h;
		h.begin = h.end = cur;
		h.bone  = "mixamorig:RightHand";
		m_move.hitboxes.push_back(h);
		m_selHitbox = (int)m_move.hitboxes.size() - 1;
	}

	const bool hasSel = (m_selHitbox >= 0 && m_selHitbox < (int)m_move.hitboxes.size());
	ImGui::BeginDisabled(!hasSel);
	ImGui::SameLine();
	if (ImGui::Button("複製##dup hitbox"))
	{
		pushUndo();
		m_move.hitboxes.push_back(m_move.hitboxes[m_selHitbox]);
		m_selHitbox = (int)m_move.hitboxes.size() - 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("削除##del hitbox"))
	{
		pushUndo();
		m_move.hitboxes.erase(m_move.hitboxes.begin() + m_selHitbox);
		m_selHitbox = std::min(m_selHitbox, (int)m_move.hitboxes.size() - 1);
	}
	ImGui::EndDisabled();

	// 一覧
	for (int i = 0; i < (int)m_move.hitboxes.size(); ++i)
	{
		const HitboxKey& h = m_move.hitboxes[i];
		char label[160];
		snprintf(label, sizeof(label), "#%d   %d〜%dF   %s   %.0f×%.0f##hb%d",
			i + 1, h.begin, h.end, ShortBoneName(h.bone), h.w, h.h, i);
		if (ImGui::Selectable(label, i == m_selHitbox)) m_selHitbox = i;
	}
	if (m_move.hitboxes.empty())
		ImGui::TextDisabled("判定を出したいフレームへ移動して「追加」を押します");

	if (m_selHitbox < 0 || m_selHitbox >= (int)m_move.hitboxes.size()) return;
	HitboxKey& h = m_move.hitboxes[m_selHitbox];

	ImGui::SeparatorText("選んでいる判定###sel hitbox");

	if (ImGui::DragIntRange2("出ているフレーム##hb frames", &h.begin, &h.end, 0.1f, 1, total, "%dF から", "%dF まで"))
	{
		h.begin = std::clamp(h.begin, 1, total);
		h.end   = std::clamp(h.end, h.begin, total);
	}
	if (ImGui::Button("開始 = 今##hb begin now")) { h.begin = cur; h.end = std::max(h.end, cur); }
	ImGui::SameLine();
	if (ImGui::Button("終了 = 今##hb end now"))   { h.end = cur; h.begin = std::min(h.begin, cur); }
	ImGui::SameLine();
	if (ImGui::Button("出始めへ移動##hb go"))     setFrame(h.begin);

	if (ImGui::BeginCombo("付けるボーン##hb bone", ShortBoneName(h.bone)))
	{
		if (ImGui::Selectable("（足元）", h.bone.empty())) h.bone.clear();
		for (const char* b : kAttachBones)
			if (ImGui::Selectable(ShortBoneName(b), h.bone == b)) h.bone = b;
		ImGui::EndCombo();
	}
	// 触り始めに控えを取るので、Ctrl+Z で数値の変更も戻せる
	float offset[2] = { h.offX, h.offY };
	const bool offChanged = ImGui::DragFloat2("ずれ（前, 上）##hb offset", offset, 0.5f, -300.0f, 300.0f, "%.1f");
	if (ImGui::IsItemActivated()) pushUndo();
	if (offChanged)
	{
		m_move.hitboxes[m_selHitbox].offX = offset[0];
		m_move.hitboxes[m_selHitbox].offY = offset[1];
	}
	float size[2] = { h.w, h.h };
	const bool sizeChanged = ImGui::DragFloat2("大きさ（幅, 高さ）##hb size", size, 0.5f, 1.0f, 400.0f, "%.1f");
	if (ImGui::IsItemActivated()) pushUndo();
	if (sizeChanged)
	{
		m_move.hitboxes[m_selHitbox].w = std::max(size[0], 1.0f);
		m_move.hitboxes[m_selHitbox].h = std::max(size[1], 1.0f);
	}
}

void MotionScene::drawViewUI()
{
	ImGui::Checkbox("床##show floor", &m_showFloor);           ImGui::SameLine();
	ImGui::Checkbox("骨格##show skeleton", &m_showSkeleton);   ImGui::SameLine();
	ImGui::Checkbox("やられ判定##show hurt", &m_showHurtbox);  ImGui::SameLine();
	ImGui::Checkbox("攻撃判定##show hit", &m_showHitbox);

	ImGui::Checkbox("ルートモーションで動かして見せる##apply root", &m_applyRoot);
	if (m_mesh && m_anim)
	{
		// 技の最後までに前へどれだけ進むか（向きは +X が前）
		float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
		const int lastKey = m_move.KeyAtFrame(totalFrames(), m_keysPerSec);
		if (m_mesh->GetRootXZ(m_move.clipBegin, x0, z0) && m_mesh->GetRootXZ(lastKey, x1, z1))
		{
			const Vector3 d = Vector3::TransformNormal(Vector3(x1 - x0, 0, z1 - z0), Matrix4x4::CreateRotationY(kFaceY));
			ImGui::Text("技の終わりまでの移動: 前 %.1f   （今 %.1f）", d.x, m_rootOffset.x);
		}
	}

	ImGui::SeparatorText("カメラ###camera presets");
	if (ImGui::Button("横（対戦画面の向き）##cam side")) setCameraView(0.0f, 0.12f);
	ImGui::SameLine();
	// キャラは +X を向いているので、正面から見るにはカメラを +X 側（yaw = +90°）に置く
	if (ImGui::Button("斜め##cam diag"))                 setCameraView(0.7f, 0.3f);
	ImGui::SameLine();
	if (ImGui::Button("正面##cam front"))                setCameraView(PI / 2.0f, 0.1f);
	ImGui::SliderFloat("距離##cam dist", &m_camDist, 80.0f, 2000.0f, "%.0f");
	ImGui::SliderFloat("見る高さ##cam target y", &m_camTarget.y, -50.0f, 300.0f, "%.0f");
	applyCamera();
}

// ================================================================ 姿勢の編集（FK）

// キーを補間した上乗せ回転。キーの間は smoothstep でゆっくり出てゆっくり止まる
Quaternion MotionScene::poseOffsetAt(const std::string& bone, int frame) const
{
	const BoneTrack* t = m_move.FindTrack(bone);
	if (!t || t->keys.empty()) return Quaternion::Identity;

	auto toQ = [](const PoseKey& k) { return Quaternion(k.q[0], k.q[1], k.q[2], k.q[3]); };
	const std::vector<PoseKey>& keys = t->keys;

	if (frame <= keys.front().frame) return toQ(keys.front());
	if (frame >= keys.back().frame)  return toQ(keys.back());

	for (size_t i = 0; i + 1 < keys.size(); ++i)
	{
		if (frame >= keys[i + 1].frame) continue;
		float u = float(frame - keys[i].frame) / float(keys[i + 1].frame - keys[i].frame);
		u = u * u * (3.0f - 2.0f * u);
		return Quaternion::Slerp(toQ(keys[i]), toQ(keys[i + 1]), u);
	}
	return toQ(keys.back());
}

void MotionScene::buildPoseOffsets()
{
	m_poseOffsets.clear();
	const int f = currentFrame();
	for (const BoneTrack& t : m_move.poseLayer)
		m_poseOffsets[t.bone] = poseOffsetAt(t.bone, f);
}

void MotionScene::pushUndo()
{
	m_undo.push_back({ m_move.poseLayer, m_move.hitboxes, Hurtbox::Defs() });
	if (m_undo.size() > 100) m_undo.erase(m_undo.begin());
}

void MotionScene::undo()
{
	if (m_undo.empty() || m_ringDrag >= 0 || m_boxDrag) return;
	UndoState s = std::move(m_undo.back());
	m_undo.pop_back();

	m_move.poseLayer = std::move(s.pose);
	m_move.hitboxes  = std::move(s.hit);
	if (m_selHitbox >= (int)m_move.hitboxes.size()) m_selHitbox = (int)m_move.hitboxes.size() - 1;

	// やられ判定は変わっていたときだけ書き戻す（書き戻すと「保存していない変更あり」になるため）
	const std::vector<HurtboxDef>& cur = Hurtbox::Defs();
	const bool same = (cur.size() == s.hurt.size()) && std::equal(cur.begin(), cur.end(), s.hurt.begin(),
		[](const HurtboxDef& a, const HurtboxDef& b) {
			return a.from == b.from && a.to == b.to && a.padX == b.padX && a.padY == b.padY && a.offX == b.offX && a.offY == b.offY;
		});
	if (!same) Hurtbox::Edit() = std::move(s.hurt);
	if (m_selHurt >= (int)Hurtbox::Defs().size()) m_selHurt = -1;

	m_status = "編集を1つ戻しました";
}

bool MotionScene::boneWorldPos(const std::string& bone, Vector3& out) const
{
	Vector3 p;
	if (!m_mesh || !m_mesh->GetBonePosition(bone, p)) return false;
	out = Vector3::Transform(p, worldMatrix());
	return true;
}

// ワールドの点 → メイン画面の中のピクセル座標（左上が原点）。カメラの後ろなら false
bool MotionScene::worldToScreen(const Vector3& p, ImVec2& out) const
{
	const Matrix4x4 vp = m_camera.GetViewMatrix() * m_camera.GetProjMatrix();
	const Vector4   c  = Vector4::Transform(Vector4(p.x, p.y, p.z, 1.0f), vp);
	if (c.w <= 1e-3f) return false;

	const ImVec2 size = ImGui::GetMainViewport()->Size;
	out.x = (c.x / c.w * 0.5f + 0.5f) * size.x;
	out.y = (0.5f - c.y / c.w * 0.5f) * size.y;
	return true;
}

// リングの軸（ワールド）。ワールド軸か、ボーン自身の軸か
bool MotionScene::ringAxisWorld(int axis, Vector3& outAxis) const
{
	static const Vector3 kUnit[3] = { Vector3::UnitX, Vector3::UnitY, Vector3::UnitZ };
	if (m_ringWorld)
	{
		outAxis = kUnit[axis];
		return true;
	}

	Matrix4x4 g;
	if (!m_mesh || !m_mesh->GetBoneGlobalMatrix(m_poseBone, g)) return false;
	outAxis = Vector3::TransformNormal(Vector3::TransformNormal(kUnit[axis], g), worldMatrix());
	outAxis.Normalize();
	return true;
}

// 軸に直交する2本。u から v へ回る向きが、その軸の「正の回転」になるように v を作る
void MotionScene::ringBasis(const Vector3& axis, Vector3& u, Vector3& v) const
{
	const Vector3 helper = (fabsf(axis.y) < 0.9f) ? Vector3::UnitY : Vector3::UnitX;
	u = axis.Cross(helper);
	u.Normalize();
	v = Vector3::TransformNormal(u, Matrix4x4::CreateFromAxisAngle(axis, PI / 2.0f));
}

void MotionScene::drawRings(const Matrix4x4& world)
{
	(void)world;
	Vector3 center;
	if (m_poseBone.empty() || !boneWorldPos(m_poseBone, center)) return;

	const float r = ringRadius();
	for (int a = 0; a < 3; ++a)
	{
		// ドラッグ中は、つかんでいる輪だけを出す（ほかの輪で見えにくくならないように）
		if (m_ringDrag >= 0 && m_ringDrag != a) continue;

		Vector3 axis, u, v;
		if (!ringAxisWorld(a, axis)) return;
		ringBasis(axis, u, v);

		const Color col = (a == m_ringDrag || (m_ringDrag < 0 && a == m_ringHover)) ? kRingHot : kRingCol[a];
		Vector3 prev = center + u * r;
		for (int i = 1; i <= kRingSegments; ++i)
		{
			const float t = 2.0f * PI * i / kRingSegments;
			const Vector3 p = center + (u * cosf(t) + v * sinf(t)) * r;
			DrawLine(prev, p, col);
			prev = p;
		}
	}
}

// 3D 画面での左クリック：リングの上ならドラッグ開始、関節の近くならその関節を選ぶ、何もなければ選択を外す
void MotionScene::updatePoseInput()
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 vpPos = ImGui::GetMainViewport()->Pos;
	const ImVec2 mouse(io.MousePos.x - vpPos.x, io.MousePos.y - vpPos.y);

	// ---- ドラッグ中：つかんだ所の接線方向に動かした量だけ回す ----
	if (m_ringDrag >= 0)
	{
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || m_poseBone.empty())
		{
			m_ringDrag = -1;
			return;
		}

		const float dx = mouse.x - m_dragStartMouse.x;
		const float dy = mouse.y - m_dragStartMouse.y;
		float angle = (dx * m_dragTangent.x + dy * m_dragTangent.y) / m_dragRadiusPx;
		if (io.KeyShift) angle *= 0.25f;						// Shift で細かく
		if (io.KeyCtrl)											// Ctrl で 5° 刻み
		{
			const float step = 5.0f * PI / 180.0f;
			angle = roundf(angle / step) * step;
		}

		// 新しい上乗せ = ボーンの向き × 軸回りの回転 × ボーンの向きの逆 × 開始時の上乗せ
		// （ボーンの今の向きに対して、その軸で回したことになる）
		Quaternion inv;
		m_dragBoneRot.Inverse(inv);
		Quaternion q = m_dragBoneRot * Quaternion::CreateFromAxisAngle(m_dragAxisModel, angle) * inv * m_dragStartOffset;
		q.Normalize();

		const float qv[4] = { q.x, q.y, q.z, q.w };
		m_move.SetPoseKey(m_poseBone, currentFrame(), qv);
		return;
	}

	m_ringHover = -1;
	if (io.WantCaptureMouse) return;	// ImGui の窓の上では何もしない

	// ---- リングの当たり：マウスから一番近い輪（8px 以内） ----
	Vector3 center;
	ImVec2  centerPx;
	int     hoverSample = 0;
	if (!m_poseBone.empty() && boneWorldPos(m_poseBone, center) && worldToScreen(center, centerPx))
	{
		float best = 8.0f;
		const float r = ringRadius();
		for (int a = 0; a < 3; ++a)
		{
			Vector3 axis, u, v;
			if (!ringAxisWorld(a, axis)) break;
			ringBasis(axis, u, v);

			ImVec2 prev;
			bool   havePrev = worldToScreen(center + u * r, prev);
			for (int i = 1; i <= kRingSegments; ++i)
			{
				const float t = 2.0f * PI * i / kRingSegments;
				ImVec2 cur;
				const bool ok = worldToScreen(center + (u * cosf(t) + v * sinf(t)) * r, cur);
				if (ok && havePrev)
				{
					const float d = DistToSegment(mouse, prev, cur);
					if (d < best) { best = d; m_ringHover = a; hoverSample = i; }
				}
				prev = cur;
				havePrev = ok;
			}
		}
	}

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

	// ---- リングをつかむ ----
	if (m_ringHover >= 0)
	{
		Vector3 axis, u, v;
		Matrix4x4 g;
		if (!ringAxisWorld(m_ringHover, axis) || !m_mesh->GetBoneGlobalMatrix(m_poseBone, g)) return;
		ringBasis(axis, u, v);

		// つかんだ所でのリングの接線（画面上）。この向きにドラッグすると正の向きに回る
		const float r  = ringRadius();
		const float t  = 2.0f * PI * hoverSample / kRingSegments;
		const float dt = 0.05f;
		ImVec2 a, b, on;
		worldToScreen(center + (u * cosf(t - dt) + v * sinf(t - dt)) * r, a);
		worldToScreen(center + (u * cosf(t + dt) + v * sinf(t + dt)) * r, b);
		worldToScreen(center + (u * cosf(t) + v * sinf(t)) * r, on);

		float tx = b.x - a.x, ty = b.y - a.y;
		const float tl = sqrtf(tx * tx + ty * ty);
		if (tl > 0.5f) { tx /= tl; ty /= tl; }
		else           { tx = 1.0f; ty = 0.0f; }	// 輪を真横から見ているとき

		const float rx = on.x - centerPx.x, ry = on.y - centerPx.y;
		m_dragRadiusPx = std::max(sqrtf(rx * rx + ry * ry), 20.0f);
		m_dragTangent  = ImVec2(tx, ty);
		m_dragStartMouse = mouse;

		// 軸をモデル空間へ（キャラは Y 軸回りに kFaceY 回して置いているので、その逆）
		m_dragAxisModel = Vector3::TransformNormal(axis, Matrix4x4::CreateRotationY(-kFaceY));
		m_dragAxisModel.Normalize();

		Vector3 s, tr;
		g.Decompose(s, m_dragBoneRot, tr);
		m_dragStartOffset = poseOffsetAt(m_poseBone, currentFrame());

		pushUndo();
		setFrame(currentFrame());	// 再生を止めて、今のフレームに固定する
		m_ringDrag = m_ringHover;
		return;
	}

	// ---- 関節を選ぶ（12px 以内で一番近いもの） ----
	float bestJoint = 12.0f;
	std::string picked;
	for (const std::string& name : PickBones())
	{
		Vector3 p;
		ImVec2  s;
		if (!boneWorldPos(name, p) || !worldToScreen(p, s)) continue;
		const float d = sqrtf((s.x - mouse.x) * (s.x - mouse.x) + (s.y - mouse.y) * (s.y - mouse.y));
		if (d < bestJoint) { bestJoint = d; picked = name; }
	}
	m_poseBone = picked;	// 何もない所をクリックしたら選択を外す
}

void MotionScene::drawPoseUI()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("3D 画面で関節（白い点）を左クリック → 色の輪をドラッグして回す。回すと今のフレームにキーが入ります。"
		"Shift 細かく / Ctrl 5°刻み / Ctrl+Z 元に戻す / Esc 選択を外す");
	ImGui::PopStyleColor();

	if (ImGui::RadioButton("ワールドの軸##ring world", m_ringWorld)) m_ringWorld = true;
	ImGui::SameLine();
	if (ImGui::RadioButton("ボーンの軸##ring local", !m_ringWorld)) m_ringWorld = false;
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("ワールド: 赤=X(前後) 緑=Y(上下) 青=Z(奥行き)の軸で回す。横から見て青の輪を回すと画面の中で回る\n"
			"ボーン: 骨自身の向きに合わせた軸で回す（ひじ・ひざの曲げ伸ばしに便利）");

	// 関節の一覧（3D 画面で選ぶのと同じ）
	if (ImGui::BeginCombo("関節##pose bone", m_poseBone.empty() ? "（選んでいません）" : ShortBoneName(m_poseBone)))
	{
		for (const std::string& name : PickBones())
		{
			char label[96];
			snprintf(label, sizeof(label), "%s%s", ShortBoneName(name), m_move.FindTrack(name) ? "  ◆" : "");
			if (ImGui::Selectable(label, name == m_poseBone)) m_poseBone = name;
		}
		ImGui::EndCombo();
	}

	if (!m_poseBone.empty())
	{
		const int cur = currentFrame();
		const BoneTrack* track = m_move.FindTrack(m_poseBone);
		const bool hasKey = m_move.FindPoseKey(m_poseBone, cur) != nullptr;

		// 今のフレームの上乗せ回転を角度で見せる。触るとこのフレームにキーが入る
		const Vector3 e = ToEulerDeg(poseOffsetAt(m_poseBone, cur));
		float ev[3] = { e.x, e.y, e.z };
		const bool changed = ImGui::DragFloat3("上乗せ回転（X, Y, Z 度）##pose euler", ev, 0.5f, -180.0f, 180.0f, "%.1f");
		if (ImGui::IsItemActivated()) pushUndo();
		if (changed)
		{
			const Quaternion q = FromEulerDeg(Vector3(ev[0], ev[1], ev[2]));
			const float qv[4] = { q.x, q.y, q.z, q.w };
			m_move.SetPoseKey(m_poseBone, cur, qv);
		}
		if (hasKey)     ImGui::TextDisabled("%dF にキーがあります", cur);
		else if (track) ImGui::TextDisabled("%dF はキーの間（補間した値）", cur);
		else            ImGui::TextDisabled("%dF  この関節はまだ編集していません", cur);

		if (ImGui::Button("キーを打つ##pose set key"))
		{
			// 今見えている値のままキーにする（補間の途中の姿勢を固定したいとき）
			pushUndo();
			const Quaternion q = poseOffsetAt(m_poseBone, cur);
			const float qv[4] = { q.x, q.y, q.z, q.w };
			m_move.SetPoseKey(m_poseBone, cur, qv);
		}
		ImGui::SameLine();
		if (ImGui::Button("元の姿勢キー##pose zero key"))
		{
			// 上乗せ 0 のキー。編集の効く範囲をここで終わらせたいときに打つ
			pushUndo();
			const float qv[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			m_move.SetPoseKey(m_poseBone, cur, qv);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasKey);
		if (ImGui::Button("キーを消す##pose del key"))
		{
			pushUndo();
			m_move.RemovePoseKey(m_poseBone, cur);
		}
		ImGui::EndDisabled();

		// キーの間を移動
		ImGui::BeginDisabled(!track);
		if (ImGui::Button("◀ 前のキー##pose prev key") && track)
		{
			for (auto it = track->keys.rbegin(); it != track->keys.rend(); ++it)
				if (it->frame < cur) { setFrame(it->frame); break; }
		}
		ImGui::SameLine();
		if (ImGui::Button("次のキー ▶##pose next key") && track)
		{
			for (const PoseKey& k : track->keys)
				if (k.frame > cur) { setFrame(k.frame); break; }
		}
		ImGui::SameLine();
		if (ImGui::Button("この関節の編集を消す##pose clear bone"))
		{
			pushUndo();
			m_move.ClearTrack(m_poseBone);
			track = nullptr;
		}
		ImGui::EndDisabled();

		// キーの一覧（押すとそのフレームへ）
		if (track)
		{
			ImGui::TextUnformatted("キー:");
			for (const PoseKey& k : track->keys)
			{
				ImGui::SameLine();
				char label[32];
				snprintf(label, sizeof(label), "%dF##pk%d", k.frame, k.frame);
				if (ImGui::SmallButton(label)) setFrame(k.frame);
			}
		}
	}

	ImGui::Separator();
	ImGui::Text("編集した関節: %d 本", (int)m_move.poseLayer.size());
	ImGui::SameLine();
	ImGui::BeginDisabled(m_undo.empty());
	char undoLabel[48];
	snprintf(undoLabel, sizeof(undoLabel), "元に戻す（%d）##pose undo", (int)m_undo.size());
	if (ImGui::Button(undoLabel)) undo();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m_move.poseLayer.empty());
	if (ImGui::Button("全部の編集を消す…##pose clear all")) ImGui::OpenPopup("姿勢の編集を全部消す##confirm pose clear");
	ImGui::EndDisabled();

	if (ImGui::BeginPopupModal("姿勢の編集を全部消す##confirm pose clear", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("この技の姿勢キーを全部消します（Ctrl+Z で戻せます）。");
		if (ImGui::Button("消す##do pose clear"))
		{
			pushUndo();
			m_move.poseLayer.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("やめる##cancel pose clear")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

// ================================================================ 判定の箱をドラッグで編集

namespace
{
	// 画面上の四角形（凸）の内側か。辺の上も内側に数える
	bool InsideQuad(const ImVec2 c[4], const ImVec2& p)
	{
		bool pos = false, neg = false;
		for (int i = 0; i < 4; ++i)
		{
			const ImVec2& a = c[i];
			const ImVec2& b = c[(i + 1) % 4];
			const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
			if (cross > 0.0f) pos = true;
			if (cross < 0.0f) neg = true;
		}
		return !(pos && neg);
	}

	// つかめる所の上では、マウスカーソルを動かせる向きの形にする
	void SetBoxCursor(int sx, int sy)
	{
		if (sx == 0 && sy == 0) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (sx == 0)       ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		else if (sy == 0)       ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else                    ImGui::SetMouseCursor(sx == sy ? ImGuiMouseCursor_ResizeNESW : ImGuiMouseCursor_ResizeNWSE);
	}
}

void MotionScene::setEditMode(EditMode mode)
{
	if (m_editMode == mode) return;
	m_editMode = mode;
	m_ringDrag = -1;
	m_boxDrag  = false;
	m_hotValid = false;

	// 触る箱が見えていないと選べないので、表示を ON にする
	if (mode == EditMode::Hitbox)  m_showHitbox  = true;
	if (mode == EditMode::Hurtbox) m_showHurtbox = true;
}

// マウス位置から視線を飛ばし、Z = planeZ の平面（戦う平面）と交わる点を求める
bool MotionScene::screenToPlane(const ImVec2& mouse, float planeZ, Vector3& out) const
{
	const ImVec2 size = ImGui::GetMainViewport()->Size;
	if (size.x < 1.0f || size.y < 1.0f) return false;
	const float nx = mouse.x / size.x * 2.0f - 1.0f;
	const float ny = 1.0f - mouse.y / size.y * 2.0f;

	const Matrix4x4 inv = (m_camera.GetViewMatrix() * m_camera.GetProjMatrix()).Invert();
	const Vector3 n = Vector3::Transform(Vector3(nx, ny, 0.0f), inv);	// 手前（ニア面）の点
	const Vector3 f = Vector3::Transform(Vector3(nx, ny, 1.0f), inv);	// 奥（ファー面）の点
	const Vector3 d = f - n;

	// 平面をほぼ真横から見ていると、少しの移動で大きく飛ぶので扱わない
	if (fabsf(d.z) < 0.05f * d.Length()) return false;
	const float t = (planeZ - n.z) / d.z;
	if (t < 0.0f) return false;
	out = n + d * t;
	return true;
}

bool MotionScene::hitRect(int i, HurtRect& r) const
{
	if (i < 0 || i >= (int)m_move.hitboxes.size()) return false;
	const HitboxKey& h = m_move.hitboxes[i];
	const Vector3 c = hitboxCenter(h, worldMatrix());
	r.x0 = c.x - h.w * 0.5f;  r.x1 = c.x + h.w * 0.5f;
	r.y0 = c.y - h.h * 0.5f;  r.y1 = c.y + h.h * 0.5f;
	return true;
}

bool MotionScene::hurtRect(int i, HurtRect& r) const
{
	const std::vector<HurtboxDef>& defs = Hurtbox::Defs();
	if (i < 0 || i >= (int)defs.size()) return false;
	Vector3 a, b;
	if (!boneWorldPos(defs[i].from, a) || !boneWorldPos(defs[i].to, b)) return false;
	r = Hurtbox::Compute(defs[i], a, b, 1.0f, Hurtbox::FacingOf(worldMatrix()));
	return true;
}

bool MotionScene::selectedRect(HurtRect& r) const
{
	if (m_editMode == EditMode::Hitbox)  return hitRect(m_selHitbox, r);
	if (m_editMode == EditMode::Hurtbox) return hurtRect(m_selHurt, r);
	return false;
}

// 長方形のどこにマウスがあるか。角（10px）→ 辺（6px）→ 中 の順に調べる
//   sx, sy : つかむ辺（-1=左/下 +1=右/上 0=そのまま）。中なら両方 0
bool MotionScene::boxHandleAt(const HurtRect& r, const ImVec2& mouse, int& sx, int& sy, bool& inside) const
{
	const float z = worldMatrix().Translation().z;
	const float xs[4] = { r.x0, r.x1, r.x1, r.x0 };		// 左下・右下・右上・左上
	const float ys[4] = { r.y0, r.y0, r.y1, r.y1 };
	ImVec2 c[4];
	for (int i = 0; i < 4; ++i)
		if (!worldToScreen(Vector3(xs[i], ys[i], z), c[i])) return false;

	sx = sy = 0;
	inside = false;

	static const int kCornerSX[4] = { -1, 1, 1, -1 }, kCornerSY[4] = { -1, -1, 1, 1 };
	for (int i = 0; i < 4; ++i)
	{
		const float dx = c[i].x - mouse.x, dy = c[i].y - mouse.y;
		if (dx * dx + dy * dy < 10.0f * 10.0f) { sx = kCornerSX[i]; sy = kCornerSY[i]; return true; }
	}

	static const int kEdgeSX[4] = { 0, 1, 0, -1 }, kEdgeSY[4] = { -1, 0, 1, 0 };	// 下・右・上・左
	float best = 6.0f;
	int   edge = -1;
	for (int i = 0; i < 4; ++i)
	{
		const float d = DistToSegment(mouse, c[i], c[(i + 1) % 4]);
		if (d < best) { best = d; edge = i; }
	}
	if (edge >= 0) { sx = kEdgeSX[edge]; sy = kEdgeSY[edge]; return true; }

	if (InsideQuad(c, mouse)) { inside = true; return true; }
	return false;
}

// ドラッグしてできた長方形を、元のデータ（ずれ・大きさ・余白）に直して書き戻す
void MotionScene::applyBoxRect(const HurtRect& r)
{
	if (m_boxKind == BoxKind::Hit)
	{
		if (m_boxIndex < 0 || m_boxIndex >= (int)m_move.hitboxes.size()) return;
		HitboxKey& h = m_move.hitboxes[m_boxIndex];

		// ずれ 0 のときの中心（付けたボーン、または足元）からの差が「ずれ」
		HitboxKey base = h;
		base.offX = base.offY = 0.0f;
		const Vector3 c0 = hitboxCenter(base, worldMatrix());

		h.w    = std::max(r.w(), 1.0f);
		h.h    = std::max(r.h(), 1.0f);
		h.offX = r.cx() - c0.x;
		h.offY = r.cy() - c0.y;
	}
	else
	{
		if (m_boxIndex < 0 || m_boxIndex >= (int)Hurtbox::Defs().size()) return;
		HurtboxDef& d = Hurtbox::Edit()[m_boxIndex];

		Vector3 a, b;
		if (!boneWorldPos(d.from, a) || !boneWorldPos(d.to, b)) return;

		// ボーン2点を囲む長方形（余白 0・ずれ 0）との差が「余白」と「ずれ」
		const float facing = Hurtbox::FacingOf(worldMatrix());
		d.padX = (r.w() - fabsf(a.x - b.x)) * 0.5f;
		d.padY = (r.h() - fabsf(a.y - b.y)) * 0.5f;
		d.offX = (r.cx() - (a.x + b.x) * 0.5f) * facing;
		d.offY =  r.cy() - (a.y + b.y) * 0.5f;
	}
}

// 3D 画面での左クリック：選んでいる箱のつまみ → 見えている箱（小さい方を優先）の順につかむ。
// 何もない所をクリックしたら選択を外す
void MotionScene::updateBoxInput()
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 vpPos = ImGui::GetMainViewport()->Pos;
	const ImVec2 mouse(io.MousePos.x - vpPos.x, io.MousePos.y - vpPos.y);
	const float  planeZ = worldMatrix().Translation().z;

	// ---- ドラッグ中：つかんだ瞬間からマウスが平面上でどれだけ動いたかで、箱を動かす・広げる ----
	if (m_boxDrag)
	{
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			m_boxDrag = false;
			return;
		}
		SetBoxCursor(m_boxSX, m_boxSY);

		Vector3 p;
		if (!screenToPlane(mouse, planeZ, p)) return;
		const float dx = p.x - m_boxStartPlane.x;
		const float dy = p.y - m_boxStartPlane.y;

		HurtRect r = m_boxStartRect;
		constexpr float kMin = 2.0f;	// これより小さくはしない
		if (m_boxSX == 0 && m_boxSY == 0)
		{
			r.x0 += dx; r.x1 += dx;
			r.y0 += dy; r.y1 += dy;
		}
		else
		{
			if (m_boxSX > 0) r.x1 = std::max(r.x1 + dx, r.x0 + kMin);
			if (m_boxSX < 0) r.x0 = std::min(r.x0 + dx, r.x1 - kMin);
			if (m_boxSY > 0) r.y1 = std::max(r.y1 + dy, r.y0 + kMin);
			if (m_boxSY < 0) r.y0 = std::min(r.y0 + dy, r.y1 - kMin);
		}
		applyBoxRect(r);
		return;
	}

	m_hotValid = false;
	if (io.WantCaptureMouse) return;	// ImGui の窓の上では何もしない

	// ---- 選んでいる箱のつまみ ----
	int  sx = 0, sy = 0;
	bool inside = false;
	HurtRect sel;
	const bool onSel = selectedRect(sel) && boxHandleAt(sel, mouse, sx, sy, inside);
	if (onSel)
	{
		m_hotValid = true;
		m_hotSX = sx;
		m_hotSY = sy;
		SetBoxCursor(sx, sy);
	}

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

	const BoxKind kind = (m_editMode == EditMode::Hitbox) ? BoxKind::Hit : BoxKind::Hurt;
	int      index = -1;
	HurtRect rect;
	if (onSel)
	{
		index = (kind == BoxKind::Hit) ? m_selHitbox : m_selHurt;
		rect  = sel;
	}
	else
	{
		// 見えている箱の中から、マウスの下にある一番小さい箱（腕の箱は胴の箱の中にあるため）
		const int f = currentFrame();
		const int n = (kind == BoxKind::Hit) ? (int)m_move.hitboxes.size() : (int)Hurtbox::Defs().size();
		float bestArea = FLT_MAX;
		for (int i = 0; i < n; ++i)
		{
			HurtRect r;
			if (kind == BoxKind::Hit)
			{
				const HitboxKey& h = m_move.hitboxes[i];
				const bool visible = (f >= h.begin && f <= h.end) || i == m_selHitbox;
				if (!visible || !hitRect(i, r)) continue;
			}
			else if (!hurtRect(i, r)) continue;

			int hx = 0, hy = 0;
			bool in = false;
			if (!boxHandleAt(r, mouse, hx, hy, in)) continue;
			const float area = r.w() * r.h();
			if (area < bestArea) { bestArea = area; index = i; rect = r; sx = hx; sy = hy; }
		}
	}

	if (kind == BoxKind::Hit) m_selHitbox = index;
	else                      m_selHurt   = index;
	if (index < 0) return;

	Vector3 p;
	if (!screenToPlane(mouse, planeZ, p)) return;	// 平面を真横から見ているときは選ぶだけ

	pushUndo();
	setFrame(currentFrame());	// 再生を止める（やられ判定はボーンの位置で形が決まるため）
	m_boxDrag       = true;
	m_boxKind       = kind;
	m_boxIndex      = index;
	m_boxSX         = sx;
	m_boxSY         = sy;
	m_boxStartPlane = p;
	m_boxStartRect  = rect;
}

// 選んでいる箱（ドラッグ中ならつかんでいる箱）の角と辺の真ん中に、つまみを出す
void MotionScene::drawBoxHandles()
{
	if (m_editMode == EditMode::Pose) return;

	HurtRect r;
	if (m_boxDrag)
	{
		const bool ok = (m_boxKind == BoxKind::Hit) ? hitRect(m_boxIndex, r) : hurtRect(m_boxIndex, r);
		if (!ok) return;
	}
	else if (!selectedRect(r)) return;

	const float z     = worldMatrix().Translation().z;
	const float s     = m_camDist * 0.012f;
	const bool  hot   = m_boxDrag || m_hotValid;
	const int   hotSX = m_boxDrag ? m_boxSX : m_hotSX;
	const int   hotSY = m_boxDrag ? m_boxSY : m_hotSY;

	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	for (int sy = -1; sy <= 1; ++sy)
	{
		for (int sx = -1; sx <= 1; ++sx)
		{
			if (sx == 0 && sy == 0) continue;
			const float x = (sx < 0) ? r.x0 : (sx > 0) ? r.x1 : r.cx();
			const float y = (sy < 0) ? r.y0 : (sy > 0) ? r.y1 : r.cy();

			// つかんでいる・マウスが乗っているつまみは大きく黄色。箱全体を動かしているときは全部黄色
			const bool isHot = hot && ((sx == hotSX && sy == hotSY) || (hotSX == 0 && hotSY == 0));
			const float size = isHot ? s * 1.6f : s;
			m_box->SetSize(size, size, size);
			m_box->Draw(Matrix4x4::CreateTranslation(Vector3(x, y, z)),
				isHot ? Color(1.0f, 0.95f, 0.2f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}
}

void MotionScene::drawHurtUI()
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextWrapped("キャラ全体で共通の形です（どの技でも、対戦画面でも同じ）。"
		"「やられ判定 (3)」にすると、3D 画面で箱の中をドラッグして移動、辺・角をドラッグして大きさを変えられます");
	ImGui::PopStyleColor();

	const std::vector<HurtboxDef>& defs = Hurtbox::Defs();
	for (int i = 0; i < (int)defs.size(); ++i)
	{
		const HurtboxDef& d = defs[i];
		char label[160];
		snprintf(label, sizeof(label), "#%d   %s → %s   余白 %.0f×%.0f##hurt%d",
			i + 1, ShortBoneName(d.from), ShortBoneName(d.to), d.padX, d.padY, i);
		if (ImGui::Selectable(label, i == m_selHurt)) m_selHurt = i;
	}

	if (m_selHurt >= 0 && m_selHurt < (int)defs.size())
	{
		ImGui::SeparatorText("選んでいるやられ判定###sel hurtbox");
		const HurtboxDef d = defs[m_selHurt];	// 触り始めに控えを取るので、値はコピーしておく

		float pad[2] = { d.padX, d.padY };
		const bool padChanged = ImGui::DragFloat2("余白（横, 縦）##hurt pad", pad, 0.5f, -100.0f, 200.0f, "%.1f");
		if (ImGui::IsItemActivated()) pushUndo();
		if (padChanged)
		{
			HurtboxDef& e = Hurtbox::Edit()[m_selHurt];
			e.padX = pad[0];
			e.padY = pad[1];
		}

		float off[2] = { d.offX, d.offY };
		const bool offChanged = ImGui::DragFloat2("ずれ（前, 上）##hurt off", off, 0.5f, -200.0f, 200.0f, "%.1f");
		if (ImGui::IsItemActivated()) pushUndo();
		if (offChanged)
		{
			HurtboxDef& e = Hurtbox::Edit()[m_selHurt];
			e.offX = off[0];
			e.offY = off[1];
		}
	}

	ImGui::Separator();
	if (ImGui::Button(Hurtbox::IsDirty() ? "保存 *##hurt save" : "保存##hurt save"))
	{
		std::string err;
		m_status = Hurtbox::Save(&err) ? "やられ判定を保存しました: HurtboxConfig.json" : "保存できません: " + err;
	}
	ImGui::SameLine();
	if (ImGui::Button("ファイルから読み直す##hurt reload"))
	{
		pushUndo();
		std::string err;
		m_status = Hurtbox::Reload(&err) ? "やられ判定を読み直しました" : "読み直せません: " + err;
	}
	ImGui::SameLine();
	if (ImGui::Button("初期値に戻す##hurt reset"))
	{
		pushUndo();
		Hurtbox::ResetDefaults();
		m_status = "やられ判定を初期値に戻しました（まだ保存していません）";
	}

	if (Hurtbox::IsDirty())
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "保存していない変更があります（対戦画面には、保存しなくても今の形が出ます）");
	if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());
}
