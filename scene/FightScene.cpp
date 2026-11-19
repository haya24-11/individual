#include    <memory>
#include	<string>
#include	<array>
#include	<vector>
#include	<cstdint>
#include	<fstream>
#include	"FightScene.h"
#include	"../system/CShader.h"
#include	"../system/imgui/imgui.h"
#include	"../system/MeshManager.h"
#include	"../system/DebugUI.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/renderer.h"
#include	"../system/meshmanager.h"
#include	"../system/CDirectInput.h"
#include	"../system/LineDrawer.h"
#include	"../system/collision.h"
#include	<algorithm>
#include	<cmath>
#include	<filesystem>
#include	<string_view>
#include	"../CameraTake.h"
#include	"../CameraLibrary.h"
#include	"../system/scenemanager.h"
#include	"Hurtbox.h"

namespace {

	struct Load3DInfo {
		std::string filename;
		std::string texdirectoryname;
		Load3DInfo(std::string p1, std::string p2) {
			filename = p1;
			texdirectoryname = p2;
		}
	};

	// パスからファイル名だけを取り出す
	// ※ std::filesystem::path は narrow 文字列を cp932 として解釈するため、
	//    /utf-8 でビルドした UTF-8 リテラルを渡すと変換に失敗して例外(1113)になる。
	//    ここでは区切り文字で切るだけにして、文字コード変換を一切行わない。
	std::string getfilename(std::string_view filestring) {
		size_t pos = filestring.find_last_of("/\\");
		std::string_view name = (pos == std::string_view::npos)
			? filestring
			: filestring.substr(pos + 1);
		return std::string(name);
	}

	std::array<Load3DInfo, 15> g_loadmodel =
	{
			Load3DInfo(
				"assets/model/car000.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/car001.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/car002.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/f1.x",				// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"assets/model/akai/akai.fbx",		// モデル名
				"assets/model/akai/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/Blue/Blue.pmx",		// モデル名
				"assets/model/Blue/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/glinco/glinco.pmx",	// モデル名
				"assets/model/glinco/"),			// テクスチャのパス

			Load3DInfo(
				"assets/model/hal/hal.pmx",			// モデル名
				"assets/model/hal/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/man/man.fbx",			// モデル名
				"assets/model/man/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/obj/goal.obj",		// モデル名
				"assets/model/obj/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/obj/cylinder.obj",	// モデル名
				"assets/model/obj/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/starwars/TIE_Fighter.x",				// モデル名
				"assets/model/starwars/texture/TIE-Fighter"),		// テクスチャのパス

			Load3DInfo(
				"assets/model/woman/woman.fbx",		// モデル名
				"assets/model/woman/"),				// テクスチャのパス

			Load3DInfo(
				"assets/model/jack1/JACK式ポプ子1.1.pmx",	// モデル名
				"assets/model/jack1/"),						// テクスチャのパス

			Load3DInfo(
				"assets/motion/X Bot.fbx",	// モデル名
				"assets/motion/")			// テクスチャのパス

			
	};

	// for debug
	void debugMeshinfo(std::string meshID) {

		CStaticMesh* smesh = MeshManager::getMesh<CStaticMesh>(meshID);
		const std::vector<VERTEX_3D>& vertices = smesh->GetVertices();
		const std::vector<uint32_t>& indices = smesh->GetIndices();
		const std::vector<SUBSET>& subsets = smesh->GetSubsets();

		{
			// テキストファイルとして開く
			std::ofstream ofs("vertices.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "Index X Y Z\n";

			for (size_t i = 0; i < vertices.size(); ++i)
			{
				const Vector3& pos = vertices[i].Position;

				ofs << i << " "
					<< pos.x << " "
					<< pos.y << " "
					<< pos.z << "\n";
			}
		}

		{
			// テキストファイルとして開く
			std::ofstream ofs("indices.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "Index \n";

			for (size_t i = 0; i < indices.size(); ++i)
			{
				const uint32_t& index = indices[i];

				ofs << i << " "
					<< index << "\n";
			}
		}

		// subsetの情報を出力
		{
			// テキストファイルとして開く
			std::ofstream ofs("subsets.txt");

			// 小数点以下6桁で出力
			ofs << std::fixed << std::setprecision(6);

			// ヘッダー
			ofs << "subset \n";

			for (size_t i = 0; i < subsets.size(); ++i)
			{
				const SUBSET& subset = subsets[i];

				ofs << i << " "
					<< subset.MaterialIdx << " "
					<< subset.IndexBase << "/"
					<< subset.IndexNum << ":"
					<< subset.VertexBase << "/"
					<< subset.VertexNum << ":"
					<< "\n";
			}
		}
	}
}

// モデルを未ロードならロード＋登録し、meshid（＝ファイル名）を返す
std::string FightScene::ensureModelLoaded(int index)
{
	std::string id = getfilename(g_loadmodel[index].filename);

	if (MeshManager::ContainsRenderer(id) == false)
	{
		// メッシュを生成
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
		mesh->Load(g_loadmodel[index].filename, g_loadmodel[index].texdirectoryname);

		// メッシュレンダラを生成
		std::unique_ptr<CStaticMeshRenderer> meshrenderer = std::make_unique<CStaticMeshRenderer>();
		meshrenderer->Init(*mesh.get());

		MeshManager::RegisterMesh<CStaticMesh>(id, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(id, std::move(meshrenderer));
	}

	return id;
}

// モデル選択（プレイヤーごとに使い回す）
void FightScene::debugModelSelect(const char* title, int& selectedIndex, std::string& meshId)
{
	// 同じウィンドウにP1/P2を並べるので、IDが衝突しないようスコープを分ける
	ImGui::PushID(title);
	ImGui::SeparatorText(title);

	// 現在選択されているモデルの名前をプレビュー用に取得（範囲外アクセスも防止）
	std::string preview_name = "なし";
	if (selectedIndex >= 0 && selectedIndex < g_loadmodel.size())
	{
		preview_name = getfilename(g_loadmodel[selectedIndex].filename);
	}

	// BeginComboを使ってドロップダウンを作成
	if (ImGui::BeginCombo("モデル##Model", preview_name.c_str()))
	{
		for (int i = 0; i < g_loadmodel.size(); ++i)
		{
			const bool is_selected = (selectedIndex == i);
			std::string item_name = getfilename(g_loadmodel[i].filename);

			// リストの各アイテムを描画し、クリックされたか判定
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selectedIndex = i;
				meshId = ensureModelLoaded(i);
			}

			// ドロップダウンを開いた時、現在選択されているアイテムにフォーカスを合わせる
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Text("選択中のモデル番号: %d", selectedIndex);

	ImGui::PopID();
}

// 全機能を1つのウィンドウにまとめて描く（タブ＋折りたたみ）
void FightScene::drawMainToolsUI()
{
	ImGui::SetNextWindowSize(ImVec2(520, 720), ImGuiCond_FirstUseEver);
	ImGui::Begin("カメラワーク ツール###Main Tools");

	// ツールのウィンドウが画面からはみ出さないようにする。
	// 前回の位置（imgui.ini）が画面の端や外だと、タイトルバーが少し見えるだけで
	// 「消えた」ように見えるため、ウィンドウ全体をモニターの作業領域（タスクバーを除く）に収める。
	// マウスで動かしている最中に押し戻すと操作しにくいので、ボタンを離している間だけ行う
	{
		const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
		if (!pio.Monitors.empty() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const ImVec2 pos  = ImGui::GetWindowPos();
			const ImVec2 size = ImGui::GetWindowSize();

			// ウィンドウの中心が乗っているモニターに収める（どれにも乗っていなければメインモニター）
			const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
			const ImGuiPlatformMonitor* mon = &pio.Monitors[0];
			for (const ImGuiPlatformMonitor& m : pio.Monitors)
			{
				if (center.x >= m.WorkPos.x && center.x < m.WorkPos.x + m.WorkSize.x &&
					center.y >= m.WorkPos.y && center.y < m.WorkPos.y + m.WorkSize.y)
				{
					mon = &m;
					break;
				}
			}

			// 画面より大きければ縮める
			const ImVec2 fit(std::min(size.x, mon->WorkSize.x), std::min(size.y, mon->WorkSize.y));
			if (fit.x != size.x || fit.y != size.y) ImGui::SetWindowSize(fit);

			// はみ出していれば内側へ押し戻す
			const ImVec2 np(
				std::clamp(pos.x, mon->WorkPos.x, mon->WorkPos.x + mon->WorkSize.x - fit.x),
				std::clamp(pos.y, mon->WorkPos.y, mon->WorkPos.y + mon->WorkSize.y - fit.y));
			if (np.x != pos.x || np.y != pos.y) ImGui::SetWindowPos(np);
		}
	}

	// モーション編集シーンへ。切り替えは次のフレームの頭で行われる
	if (ImGui::Button("モーション編集へ →##to motion")) SceneManager::RequestScene("MotionScene");

	if (ImGui::BeginTabBar("##MainTabs"))
	{
		// ---------------- カメラ ----------------
		if (ImGui::BeginTabItem("カメラ##Camera"))
		{
			int mode = (int)m_camMode;
			if (ImGui::RadioButton("対戦（基本）##Fighting (basic)", &mode, (int)CamMode::Fighting)) {
				m_camMode = CamMode::Fighting;
				m_splineCam.SetActive(false);
				m_fightCam.ResetSnap();
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("スプライン（曲線）##Spline (curve)", &mode, (int)CamMode::Spline)) {
				m_camMode = CamMode::Spline;
				m_splineCam.Play();			// 先頭から
			}
			ImGui::Text("Cキー: 対戦カメラとスプラインカメラを切り替え");

			ImGui::Separator();
			ImGui::Checkbox("手動フリーカメラ（メイン画面）##Manual free-fly", &m_manualCam);
			ImGui::Checkbox("デバッグビュー（フリーカメラ画面）##Debug view", &m_debugViewOpen);

			if (ImGui::CollapsingHeader("対戦カメラのパラメータ##Fighting Params",
				ImGuiTreeNodeFlags_DefaultOpen))
			{
				m_fightCam.DrawUI();
			}

			if (ImGui::CollapsingHeader("被写界深度（DOF）##DOF", ImGuiTreeNodeFlags_DefaultOpen))
			{
				m_dof.DrawUI();
			}

			ImGui::EndTabItem();
		}

		// ---------------- スプライン ----------------
		if (ImGui::BeginTabItem("スプライン##Spline"))
		{
			if (ImGui::CollapsingHeader("再生設定##Playback", ImGuiTreeNodeFlags_DefaultOpen))
				m_splineCam.DrawPlaybackUI();

			if (ImGui::CollapsingHeader("曲線エディター##CurveEditor", ImGuiTreeNodeFlags_DefaultOpen))
				m_splineCam.DrawCurveEditorUI();

			if (ImGui::CollapsingHeader("速度##Speed", ImGuiTreeNodeFlags_DefaultOpen))
				m_splineCam.DrawSpeedUI();

			if (ImGui::CollapsingHeader("ロール（ダッチアングル）##Roll"))
				m_splineCam.DrawRollUI();

			ImGui::EndTabItem();
		}

		// ---------------- ライブラリ ----------------
		if (ImGui::BeginTabItem("ライブラリ##Library"))
		{
			drawLibraryUI();
			ImGui::EndTabItem();
		}

		// ---------------- モデル ----------------
		if (ImGui::BeginTabItem("モデル##Model"))
		{
			debugModelSelect("プレイヤー1 モデル###Player1 Model", m_p1Select, m_meshid);
			debugModelSelect("プレイヤー2 モデル###Player2 Model", m_p2Select, m_meshid2);
			drawMotionUI();
			ImGui::EndTabItem();
		}

		// ---------------- デバッグ ----------------
		if (ImGui::BeginTabItem("デバッグ##Debug"))
		{
			ImGui::Checkbox("当たり判定を表示##Show hitbox", &m_showHitbox);
			ImGui::SliderFloat("線の濃さ##Hitbox alpha", &m_hitboxAlpha, 0.05f, 1.0f, "%.2f");
			ImGui::TextDisabled("緑＝やられ判定　青＝押し合い判定　（まだ判定は行っていません）");
			ImGui::Separator();
			ImGui::SliderFloat("壁の奥行き##Wall Z", &m_wallZ, 100.0f, 1000.0f, "%.0f");
			ImGui::Separator();

			// リムライト（キャラの輪郭光）。値を変えたらその場でシェーダーへ送り直す
			{
				RIMLIGHT rim = Renderer::GetRimLight();
				bool enable  = rim.Enable != FALSE;
				bool changed = false;
				changed |= ImGui::Checkbox("リムライト##Rim enable", &enable);
				changed |= ImGui::ColorEdit3("縁の色##Rim color", &rim.RimColor.x);
				changed |= ImGui::SliderFloat("縁の細さ##Rim power", &rim.Power, 0.5f, 8.0f, "%.1f");
				changed |= ImGui::SliderFloat("縁の明るさ##Rim intensity", &rim.Intensity, 0.0f, 3.0f, "%.2f");
				if (changed)
				{
					rim.Enable = enable ? TRUE : FALSE;
					Renderer::SetRimLight(rim);
				}
			}
			ImGui::Separator();
			debugRubikCubeLocalRotation();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// ローカル軸回転
void FightScene::debugRubikCubeLocalRotation()
{
	Vector3 inputangle = { 0.0f,0.0f,0.0f };

	ImGui::SliderFloat("X軸回転##X Rotation", &inputangle.x, 0.0f, PI);
	ImGui::SliderFloat("Y軸回転##Y Rotation", &inputangle.y, 0.0f, PI);
	ImGui::SliderFloat("Z軸回転##Z Rotation", &inputangle.z, 0.0f, PI);

	// ローカル軸を取得
	Vector3 up = m_RotationMtx.Up();
	Vector3 right = m_RotationMtx.Right();
	Vector3 forward = m_RotationMtx.Forward();

	Quaternion qy = Quaternion::CreateFromAxisAngle(up, inputangle.y);
	Quaternion qx = Quaternion::CreateFromAxisAngle(right, inputangle.x);
	Quaternion qz = Quaternion::CreateFromAxisAngle(forward, inputangle.z);

	m_RotationQ = m_RotationQ * qx * qy * qz;

	m_RotationMtx = Matrix4x4::CreateFromQuaternion(m_RotationQ);

	static Vector3 scale = { 1.0f,1.0f,1.0f };

	ImGui::SliderFloat("X軸拡大率##X scale", &scale.x, 0.5f, 20.0f);
	ImGui::SliderFloat("Y軸拡大率##Y scale", &scale.y, 0.5f, 20.0f);
	ImGui::SliderFloat("Z軸拡大率##Z scale", &scale.z, 0.5f, 20.0f);

	m_ScaleMtx = Matrix4x4::CreateScale(scale);

	// Player1 専用スケール（Player2には影響しない）
	ImGui::SliderFloat("プレイヤー1の拡大率##P1 scale", &m_p1Scale, 1.0f, 30.0f);
}

FightScene::FightScene()
{

}

// マウス位置からワールド空間のレイ（始点・方向）を計算する
bool FightScene::computeMouseRay(Vector3& outOrigin, Vector3& outDir) const
{
	float w = (float)Application::GetWidth();
	float h = (float)Application::GetHeight();

	CDirectInput& in = CDirectInput::GetInstance();
	int mx = in.GetMousePosX();
	int my = in.GetMousePosY();

	float ndcX =  (2.0f * mx / w) - 1.0f;
	float ndcY = 1.0f - (2.0f * my / h);

	Matrix4x4 vp = m_camera.GetViewMatrix() * m_camera.GetProjMatrix();
	Matrix4x4 invVP = vp.Invert();

	Vector3 nearP = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invVP);
	Vector3 farP  = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), invVP);

	outOrigin = nearP;
	outDir = farP - nearP;
	outDir.Normalize();
	return true;
}

// 選択対象の実体（Vector3*）を返す
Vector3* FightScene::getPickTarget(PickKind kind, int index)
{
	if (kind == PickKind::ControlPoint)
	{
		if (index >= 0 && index < m_splineCam.PointCount()) return &m_splineCam.Points()[index];
		return nullptr;
	}
	if (kind == PickKind::Lookat)
	{
		return &m_splineCam.Lookat();
	}
	return nullptr;
}

// 制御点・注視点のクリック選択とドラッグ移動を処理する
void FightScene::updateGizmoPicking()
{
	namespace Collision = GM31::GE::Collision;	// system/C3DShape.h の Segment クラスと名前が衝突するため別名で修飾

	if (ImGui::GetIO().WantCaptureMouse) return;

	CDirectInput& in = CDirectInput::GetInstance();

	Vector3 rayOrigin, rayDir;
	computeMouseRay(rayOrigin, rayDir);

	// --- ドラッグ中：軸上でマウスレイに最も近い点へ移動 ---
	if (m_dragAxis != GizmoAxis::None)
	{
		if (!in.GetMouseLButtonCheck())
		{
			m_dragAxis = GizmoAxis::None;
			return;
		}

		Vector3* target = getPickTarget(m_selectedKind, m_selectedIndex);
		if (!target)
		{
			m_dragAxis = GizmoAxis::None;
			return;
		}

		Vector3 axisDir =
			(m_dragAxis == GizmoAxis::X) ? Vector3(1, 0, 0) :
			(m_dragAxis == GizmoAxis::Y) ? Vector3(0, 1, 0) :
											Vector3(0, 0, 1);

		Collision::Line rayLine{ rayOrigin, rayDir };
		Collision::Line axisLine{ *target, axisDir };

		float s, t;
		Vector3 p1, p2;
		Collision::ClosestDistanceBetweenLines(rayLine, axisLine, s, t, p1, p2);

		*target = p2;
		return;
	}

	// --- クリックした瞬間だけ判定 ---
	if (!in.GetMouseLButtonTrigger()) return;

	// 1. 選択中の点があれば、まずギズモの軸に当たっているか判定
	if (m_selectedKind != PickKind::None)
	{
		Vector3* target = getPickTarget(m_selectedKind, m_selectedIndex);
		if (target)
		{
			const float axisLen = m_gizmoShaftLen + m_gizmoHeadLen;

			struct AxisDef { GizmoAxis axis; Vector3 dir; };
			AxisDef axes[3] = {
				{ GizmoAxis::X, Vector3(1, 0, 0) },
				{ GizmoAxis::Y, Vector3(0, 1, 0) },
				{ GizmoAxis::Z, Vector3(0, 0, 1) },
			};

			GizmoAxis best = GizmoAxis::None;
			float bestDist = m_gizmoPickThresh;

			for (auto& a : axes)
			{
				Collision::Line rayLine{ rayOrigin, rayDir };
				Collision::Line axisLine{ *target, a.dir };

				float s, t;
				Vector3 p1, p2;
				float dist = Collision::ClosestDistanceBetweenLines(rayLine, axisLine, s, t, p1, p2);

				if (t >= 0.0f && t <= axisLen && dist < bestDist)
				{
					bestDist = dist;
					best = a.axis;
				}
			}

			if (best != GizmoAxis::None)
			{
				m_dragAxis = best;
				return;	// ドラッグ開始。選択は維持
			}
		}
	}

	// 2. 軸に当たらなければ、全対象球への当たり判定で選択
	Vector3 rayEnd = rayOrigin + rayDir * 20000.0f;
	Collision::Segment raySeg{ rayOrigin, rayEnd };

	PickKind bestKind  = PickKind::None;
	int      bestIndex = -1;
	float    bestT     = 2.0f;	// t∈[0,1]の外側を初期値に

	std::vector<Vector3>& points = m_splineCam.Points();
	for (int i = 0; i < (int)points.size(); ++i)
	{
		Vector3 hit; float t;
		float dist = Collision::calcPointLineDist(points[i], raySeg, hit, t);
		if (dist <= 12.0f && t >= 0.0f && t <= 1.0f && t < bestT)
		{
			bestT = t;
			bestKind = PickKind::ControlPoint;
			bestIndex = i;
		}
	}
	{
		Vector3 hit; float t;
		float dist = Collision::calcPointLineDist(m_splineCam.Lookat(), raySeg, hit, t);
		if (dist <= 15.0f && t >= 0.0f && t <= 1.0f && t < bestT)
		{
			bestT = t;
			bestKind = PickKind::Lookat;
			bestIndex = -1;
		}
	}

	m_selectedKind = bestKind;
	m_selectedIndex = bestIndex;
}

// 選択中の点に X(赤)/Y(緑)/Z(青) の矢印ギズモを描画するz
void FightScene::drawGizmo()
{
	if (m_selectedKind == PickKind::None) return;

	Vector3* target = getPickTarget(m_selectedKind, m_selectedIndex);
	if (!target) return;

	Vector3 origin = *target;

	struct AxisDef { Matrix4x4 rot; Color col; };
	AxisDef axes[3] = {
		{ Matrix4x4::CreateRotationZ(-PI / 2.0f), Color(1, 0, 0, 1) },	// X軸：赤
		{ Matrix4x4::Identity,                    Color(0, 1, 0, 1) },	// Y軸：緑
		{ Matrix4x4::CreateRotationX(PI / 2.0f),  Color(0, 0, 1, 1) },	// Z軸：青
	};

	for (auto& a : axes)
	{
		Matrix4x4 base = a.rot * Matrix4x4::CreateTranslation(origin);

		if (m_gizmoShaft) m_gizmoShaft->Draw(base, a.col);

		Matrix4x4 headOffset = Matrix4x4::CreateTranslation(0, m_gizmoShaftLen, 0);
		if (m_gizmoHead) m_gizmoHead->Draw(headOffset * base, a.col);
	}
}

void FightScene::update(uint64_t deltatime)
{
	// マイクロ秒 → 秒
	float dt = deltatime / 1'000'000.0f;

	CDirectInput& in = CDirectInput::GetInstance();

	// 制御点・注視点のギズモ操作（スプラインカメラの再生状態に関わらず動作させる）
	updateGizmoPicking();

	// --- Player1 アッパーカットモーション（J キーで再生開始、終了で構えに戻る） ---
	if (in.CheckKeyBufferTrigger(DIK_J) && m_p1Loop)
	{
		playMotion(m_p1AttackName, false);
	}
	if (m_p1AnimMesh&&m_p1Current)
	{
		m_p1AnimFrame += m_animFps * m_p1SpeedScale * dt;
		const float last = (float)m_p1FrameEnd;
		if (m_p1AnimFrame >= last)
		{
			if (m_p1Loop)
			{
				m_p1AnimFrame = (float)m_p1FrameBegin;	//範囲の先頭へ戻す
				m_p1RootValid = false;					//折り返しの巨大な差分を無視する
			}
			else if (!playMotion(m_p1IdleName, true))				//技が終わったら構えへ
			{
				m_p1AnimFrame = last;
				m_p1Loop = true;	// 構えに戻れなくても入力を受け付け続ける
			}
		}
		int frame = (int)m_p1AnimFrame;

		// ルートモーションを位置へ反映する。見た目はその場再生のまま、キャラだけが動く
		// 構えは腰が前後にわずかに揺れるだけなので移動させない（カメラが揺れる原因になる）
		if (m_p1ApplyRootMotion && m_p1CurrentName != m_p1IdleName)
		{
			float rx = 0.0f, rz = 0.0f;
			if (m_p1AnimMesh->GetRootXZ(frame, rx, rz))
			{
				// 1フレームで進む量は走りでも約7。これを大きく超える差分は
				// ループの折り返しやキー数の食い違いによるものなので捨てる
				const float dx = rx - m_p1RootPrevX;
				const float dz = rz - m_p1RootPrevZ;
				const bool  sane = (fabsf(dx) < 50.0f && fabsf(dz) < 50.0f);

				if (m_p1RootValid && sane)
				{
					// モデル空間の差分を、描画と同じスケール・向きでワールドへ移す
					Vector3 local(dx * m_p1Scale, 0.0f, dz * m_p1Scale);
					m_p1Pos += Vector3::TransformNormal(local, Matrix4x4::CreateRotationY(m_p1FaceY));
				}
				m_p1RootPrevX = rx;
				m_p1RootPrevZ = rz;
				m_p1RootValid = true;
			}
		}

		m_p1AnimMesh->Update(m_p1BoneComb, frame);
	}

	// --- Player2 の構え（常にループ。P1 と同じ再生速度） ---
	if (m_p2AnimMesh && m_p2Current)
	{
		m_p2AnimFrame += m_animFps * dt;
		const float last = (float)std::max(m_p2CurrentFrames - 1, 0);
		if (m_p2AnimFrame >= last) m_p2AnimFrame = 0.0f;

		int frame = (int)m_p2AnimFrame;
		m_p2AnimMesh->Update(m_p2BoneComb, frame);
	}
	// 姿勢を毎フレーム計算（非再生時はフレーム0の構えを維持）
	if (m_p1AnimMesh)
	{
		int frame = (int)m_p1AnimFrame;
	}

	// --- C キー: メインカメラを 基本 ⇔ スプライン でトグル ---
	if (in.CheckKeyBufferTrigger(DIK_C))
	{
		m_camMode = (m_camMode == CamMode::Spline) ? CamMode::Fighting : CamMode::Spline;

		// スプラインへ切り替えたときは必ず先頭から流す。
		// ループOFFだと終端で止まったままになり、切り替えても動かないため。
		if (m_camMode == CamMode::Spline) m_splineCam.Play();
		else {
			m_splineCam.SetActive(false);
			m_fightCam.ResetSnap();		// 復帰時は即スナップ
		}
	}

	// デバッグ第2ビューのカメラ操作はビュー窓のImGuiラムダ内で行う（別OSウィンドウでも効くように）

	// --- メインカメラを m_camera に反映する ---
	if (m_manualCam)
	{
		if (!m_prevManualCam)								// 手動ON開始時は現在姿勢を引き継ぐ
			m_manualFly.AdoptFromLookAt(m_camera.GetPosition(), m_camera.GetLookat());
		m_manualFly.Update(dt, m_camera);					// 手動フリー操作（WASD/右ドラッグ）
	}
	else if (m_camMode == CamMode::Spline)
	{
		m_splineCam.Update(dt, m_camera);					// 曲線カメラ
	}
	else
	{
		m_fightCam.Update(dt, m_camera, m_p1Pos, m_p2Pos);	// 格ゲー基本カメラ
	}
	m_prevManualCam = m_manualCam;
}

void FightScene::draw(uint64_t deltatime)
{
	// オートフォーカス：2人の中間（胸の高さ）にピントを合わせる。
	// 深度バッファが持つのは「カメラの正面方向の奥行き」なので、直線距離ではなく視線方向の成分を使う
	if (m_dof.IsAutoFocus())
	{
		const Vector3 eye = m_camera.GetPosition();
		Vector3       fwd = m_camera.GetLookat() - eye;
		const Vector3 target = (m_p1Pos + m_p2Pos) * 0.5f + Vector3(0.0f, 120.0f, 0.0f);
		if (fwd.LengthSquared() > 1e-6f)
		{
			fwd.Normalize();
			m_dof.SetFocusDistance(std::max((target - eye).Dot(fwd), 1.0f));
		}
	}

	// メインパス。DOF が ON ならオフスクリーンに描いてから、ぼかして画面へ出す
	m_camera.Draw();
	m_dof.BeginScene();
	drawSceneGeometry();
	m_dof.EndScene();

	// デバッグ第2ビュー（開いている時だけ free-fly 視点でオフスクリーンへ再描画）
	if (m_debugViewOpen && m_dbgRTV) renderDebugView();
}

// シーンの3D描画本体（メイン／第2ビュー共通。view/projは呼び出し前に設定済み前提）
void FightScene::drawSceneGeometry()
{
	// 3軸カラー
	Color axiscol[3] = {
		Color(1, 0, 0, 1), 
		Color(0, 1, 0, 1),
		Color(0, 1, 1, 1)
	};

	// 3本のworld軸を描画
	for (int cnt = 0; cnt < m_segments.size(); cnt++)
	{
		Matrix4x4 worldmtx = Matrix4x4::Identity;
		m_segments[cnt]->SetWidth(3);
		m_segments[cnt]->Draw(worldmtx, Color(0,0,0,1));
	}

	// 3本のローカル軸を描画
	for (int cnt=0;cnt<m_segments.size();cnt++)
	{
		m_segments[cnt]->SetWidth(3);
		m_segments[cnt]->Draw(m_RotationMtx, axiscol[cnt]);
	}

	// player1 を描画（左・相手を向く）: suzu（静的PMX）。アッパーカットのアニメは一旦停止。
	// PMXはMMDスケール（約20単位）なので m_p1Scale（既定9倍）で拡大して表示する。
	Matrix4x4 w1 = Matrix4x4::CreateScale(m_p1Scale) * Matrix4x4::CreateRotationY(m_p1FaceY) * Matrix4x4::CreateTranslation(m_p1Pos);
	Renderer::SetWorldMatrix(&w1);
	
	if (m_p1AnimMesh && m_p1Current && m_meshid == "X Bot.fbx")
	{

		ShaderManager::Get<CShader>("Skin3D")->SetGPU();
		m_p1BoneComb.Update();
		m_p1BoneComb.SetGPU();
		m_p1AnimMesh->Draw();
	}
	else
	{
		ShaderManager::Get<CShader>("Shader3D")->SetGPU();
		if (auto* r1 = MeshManager::getRenderer <CStaticMeshRenderer>(m_meshid)) r1->Draw();
	}
	


	// player2 を描画（右・相手を向く）: P1 と同じくスキンメッシュ／静的で分岐する
	Matrix4x4 w2 = m_ScaleMtx * Matrix4x4::CreateRotationY(m_p2FaceY) * Matrix4x4::CreateTranslation(m_p2Pos);
	Renderer::SetWorldMatrix(&w2);

	if (m_p2AnimMesh && m_p2Current && m_meshid2 == "X Bot.fbx")
	{
		ShaderManager::Get<CShader>("Skin3D")->SetGPU();
		m_p2BoneComb.Update();
		m_p2BoneComb.SetGPU();
		m_p2AnimMesh->Draw();
	}
	else
	{
		ShaderManager::Get<CShader>("Shader3D")->SetGPU();
		if (auto* r2 = MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid2)) r2->Draw();
	}

	// この先の可視化は静的シェーダ前提なので戻しておく
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();

	// スプライン経路・マーカーの可視化
	m_splineCam.DrawVisualization();

	// 選択中の制御点・注視点に軸ギズモ（矢印）を描画
	drawGizmo();

	// 板ポリ（草の地面）を描画（現在の3Dカメラのview/projを使用してワールド空間に配置）
	// 1枚ポリゴンはどちらの面からでも見えるようカリングを無効化（両面描画）してから描く
	if (m_sprite) {
		Renderer::DisableCulling(false);	// カリングOFF（両面）
		// X軸90度回転で水平化、モデル足元(y=-100)へ配置
		m_sprite->Draw(Vector3(1, 1, 1), Vector3(PI / 2.0f, 0, 0), Vector3(0, -100, 0));
		Renderer::DisableCulling(true);		// カリングON（通常）に戻す
	}

	// 奥の壁。板ポリはもともと縦向き（XY平面）なので回転なしで立つ。
	// 中心の高さを「地面 + 高さの半分」にして、下端を地面にぴったり付ける
	if (m_wall) {
		Renderer::DisableCulling(false);	// 裏側に回り込んでも見えるよう両面
		const float floorY = -100.0f;
		m_wall->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0),
			Vector3(0, floorY + m_wall->GetHeight() * 0.5f, m_wallZ));
		Renderer::DisableCulling(true);
	}

	// 当たり判定の可視化（デバッグタブで ON にしたときだけ）
	// 枠線だけ・深度テスト無しで最後に描くことで、常に全部が手前に出る
	if (m_showHitbox)
	{
		ShaderManager::Get<CShader>("Shader3D")->SetGPU();
		Renderer::SetDepthEnable(false);				// 奥行きを無視して手前に重ねる
		Renderer::SetFillMode(D3D11_FILL_WIREFRAME);	// 枠線だけ

		const Color green(0.1f, 1.0f, 0.1f, m_hitboxAlpha);	// やられ判定
		const Color blue (0.2f, 0.4f, 1.0f, m_hitboxAlpha);	// 押し合い判定

		drawHurtboxes(m_p1AnimMesh.get(), w1, m_p1Scale, green);
		drawPushbox  (m_p1AnimMesh.get(), w1, m_p1Scale, blue);
		drawHurtboxes(m_p2AnimMesh.get(), w2, 1.0f,      green);
		drawPushbox  (m_p2AnimMesh.get(), w2, 1.0f,      blue);

		Renderer::SetFillMode(D3D11_FILL_SOLID);		// 通常の塗りに戻す
		Renderer::SetDepthEnable(true);					// 深度テストを戻す
	}
}

// デバッグ第2ビュー用のオフスクリーンRT（カラー＋深度）を生成
void FightScene::createDebugTarget()
{
	m_dbgW = (UINT)Application::GetWidth();	// フル解像度（アプリと同アスペクト・拡大に強い）
	m_dbgH = (UINT)Application::GetHeight();
	if (m_dbgW < 1 || m_dbgH < 1) return;

	ID3D11Device* dev = Renderer::GetDevice();

	// カラー（RT兼SRV）
	D3D11_TEXTURE2D_DESC cd{};
	cd.Width = m_dbgW; cd.Height = m_dbgH; cd.MipLevels = 1; cd.ArraySize = 1;
	cd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; cd.SampleDesc.Count = 1;
	cd.Usage = D3D11_USAGE_DEFAULT;
	cd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(dev->CreateTexture2D(&cd, nullptr, m_dbgColorTex.GetAddressOf()))) return;
	if (FAILED(dev->CreateRenderTargetView(m_dbgColorTex.Get(), nullptr, m_dbgRTV.GetAddressOf()))) return;
	if (FAILED(dev->CreateShaderResourceView(m_dbgColorTex.Get(), nullptr, m_dbgSRV.GetAddressOf()))) return;

	// 深度
	D3D11_TEXTURE2D_DESC dd{};
	dd.Width = m_dbgW; dd.Height = m_dbgH; dd.MipLevels = 1; dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dd.SampleDesc.Count = 1;
	dd.Usage = D3D11_USAGE_DEFAULT; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(dev->CreateTexture2D(&dd, nullptr, m_dbgDepthTex.GetAddressOf()))) return;
	if (FAILED(dev->CreateDepthStencilView(m_dbgDepthTex.Get(), nullptr, m_dbgDSV.GetAddressOf()))) return;
}

// free-fly 視点でシーンをオフスクリーンへ再描画する（第2パス）
void FightScene::renderDebugView()
{
	ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

	// 現在の RT／ビューポートを保存（OMGetRenderTargets は参照を返すので後で Release）
	ID3D11RenderTargetView* sRTV = nullptr;
	ID3D11DepthStencilView* sDSV = nullptr;
	ctx->OMGetRenderTargets(1, &sRTV, &sDSV);
	UINT nvp = 1; D3D11_VIEWPORT sVP{};
	ctx->RSGetViewports(&nvp, &sVP);

	// オフスクリーンへ切替＋クリア
	ID3D11RenderTargetView* rtv = m_dbgRTV.Get();
	ctx->OMSetRenderTargets(1, &rtv, m_dbgDSV.Get());
	D3D11_VIEWPORT vp{};
	vp.Width = (float)m_dbgW; vp.Height = (float)m_dbgH; vp.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &vp);
	const float clr[4] = { 0.10f, 0.10f, 0.12f, 1.0f };
	ctx->ClearRenderTargetView(rtv, clr);
	ctx->ClearDepthStencilView(m_dbgDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// free-fly 視点で再描画
	m_debugCam.Draw();			// view/proj を設定（アスペクトは App w/h＝オフスクリーンと同一）
	drawSceneGeometry();

	// メイン RT／ビューポートを復元（戻さないと後段の ImGui 描画・present が壊れる）
	ctx->OMSetRenderTargets(1, &sRTV, sDSV);
	ctx->RSSetViewports(1, &sVP);
	if (sRTV) sRTV->Release();
	if (sDSV) sDSV->Release();
}

void FightScene::init()
{
	refreshLibraryLists();		// ライブラリの一覧を読み込む

	// カメラ(3D)の初期化
	m_camera.Init();
	m_camera.SetPosition(Vector3(0, 0, -300));
	m_camera.SetLookat(Vector3(0, 0, 0));
	m_camera.SetUP(Vector3(0, 1, 0));

	// フリーカメラの初期姿勢（デバッグ第2ビューの初期視点）
	m_freeCam.AdoptFromLookAt(Vector3(0, 0, -300), Vector3(0, 0, 0));
	m_debugCam.Init();
	m_debugCam.SetPosition(Vector3(0, 0, -300));	// 初回フレーム描画用の初期姿勢
	m_debugCam.SetLookat(Vector3(0, 0, 0));
	m_debugCam.SetUP(Vector3(0, 1, 0));

	// デバッグ第2ビュー用オフスクリーンRTを生成
	createDebugTarget();

	//被写界深度
	m_dof.Init((UINT)Application::GetWidth(), (UINT)Application::GetHeight());


	// ローカル軸表示用線分の初期化
	m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
	m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
	m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));

	// クオータニオンに単位クオータニオンをセット
	m_RotationQ = Quaternion::Identity;

	// シェーダを生成
	std::unique_ptr<CShader> shader{};
	shader = std::make_unique<CShader>();
	shader->Create(
		"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
		"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー
	);
	ShaderManager::Register<CShader>("Shader3D", std::move(shader));

	// player1 / player2 の初期モデルを読み込み
	m_meshid  = ensureModelLoaded(m_p1Select);
	m_meshid2 = ensureModelLoaded(m_p2Select);

	// --- Player1 アニメーション（アッパーカット試作） ---

	// スキニング用シェーダ（PSは静的と共通）
	std::unique_ptr<CShader> skinShader = std::make_unique<CShader>();
	skinShader->Create(
		"shader/vertexLightingOneSkinVS.hlsl",	// スキニング頂点シェーダー
		"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー（静的と共通）
	);
	ShaderManager::Register<CShader>("Skin3D", std::move(skinShader));

	// X Bot（スキン+ボーン）を読み込み
	m_p1AnimMesh = std::make_unique<CAnimationMeshBlender>();
	m_p1AnimMesh->Load("assets/motion/X Bot.fbx", "assets/motion/");
	m_p1AnimMesh->SetLockRootXZ(true);	// 歩き・走りをその場で再生する（キャラが流れていかない）

	// モーション一覧を取得して構えを再生（各 FBX の読み込みは選択時に遅延）
	scanMotionFolder();
	if (!playMotion(m_p1IdleName, true))
		playMotion("Uppercut Jab", true);	// パックが無い環境でも何か動くように

	// ボーン行列定数バッファ(b5)を生成
	m_p1BoneComb.Create();

	// プレイヤー2 も同じスキンメッシュを読み込み、構えをループ再生する
	m_p2AnimMesh = std::make_unique<CAnimationMeshBlender>();
	m_p2AnimMesh->Load("assets/motion/X Bot.fbx", "assets/motion/");
	m_p2AnimMesh->SetLockRootXZ(true);
	playMotionP2(m_p2IdleName);
	m_p2BoneComb.Create();

	// 板ポリ（床タイルの地面）を生成：大きめサイズ＋UVを繰り返してタイリング
	const float tile = 10.0f;	// タイルを10×10回繰り返す
	std::array<Vector2, 4> grounduv = {
		Vector2(0, 0), Vector2(tile, 0), Vector2(0, tile), Vector2(tile, tile)
	};
	m_sprite = std::make_unique<CSprite>(2000, 2000, "assets/texture/floortile.png", grounduv);

	// 奥の壁：地面と同じテクスチャ。タイル1枚の大きさを地面と揃える（2000/10 = 200 単位）
	// 横2000・高さ1000 なので、横10枚・縦5枚
	std::array<Vector2, 4> walluv = {
		Vector2(0, 0), Vector2(tile, 0), Vector2(0, tile * 0.5f), Vector2(tile, tile * 0.5f)
	};
	m_wall = std::make_unique<CSprite>(2000, 1000, "assets/texture/floortile.png", walluv);


	// スプラインカメラ：既定の制御点・マーカーを生成
	m_splineCam.Init();

	// 全機能を1つのウィンドウにまとめて登録する（タブ＋折りたたみ）
	// this を渡すと、このシーンが表示中のときだけ描かれる（モーション編集シーンへ移ると隠れる）
	DebugUI::RedistDebugFunction([this]() {
		drawMainToolsUI();
		}, this);

	// デバッグ第2ビュー窓（free-fly視点のオフスクリーンをImGuiに表示）
	DebugUI::RedistDebugFunction([this]() {
		if (!m_debugViewOpen || !m_dbgSRV) return;
		ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_FirstUseEver);	// 初回の既定サイズ
		ImGui::Begin("デバッグ・フリーカメラビュー###Debug Free-fly View");
		ImGui::TextDisabled("フォーカス中: WASDで移動／右ドラッグで視点操作／Q・Eで上下移動／Shiftで3倍速");

		// 映像をウィンドウのサイズに合わせて拡大（アスペクト維持で歪ませない）
		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x < 1) avail.x = 1;
		if (avail.y < 1) avail.y = 1;
		float aspect = (m_dbgH > 0) ? (float)m_dbgW / (float)m_dbgH : 1.0f;
		float w = avail.x, h = w / aspect;			// 幅基準、はみ出たら高さ基準に
		if (h > avail.y) { h = avail.y; w = h * aspect; }
		ImGui::Image((ImTextureID)(size_t)m_dbgSRV.Get(), ImVec2(w, h));
		// この窓を操作中(フォーカス/ホバー)のときだけ free-fly を動かす＝窓にカメラが付く
		bool active = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();
		m_freeCam.UpdateImGui(m_debugCam, active);
		ImGui::End();
		}, this);

	// 軸ギズモ（矢印＝円柱の軸＋円錐の先端）
	m_gizmoShaft = std::make_unique<Cylinder>(m_gizmoShaftRad, m_gizmoShaftLen);
	m_gizmoHead  = std::make_unique<Cone>(m_gizmoHeadRad, m_gizmoHeadLen);

	// 当たり判定の可視化用。サイズは描画のたびに設定するので初期値は仮
	m_hitBox = std::make_unique<Box>(10.0f, 10.0f, 10.0f);


}

void FightScene::dispose()
{

}

void  FightScene::scanMotionFolder() 
{
	namespace fs = std::filesystem;
	m_motionList.clear();

	const fs::path root = "assets/motion";
	if (!fs::exists(root))return;

	for (const auto& e :fs::recursive_directory_iterator(root))
	{
		if (!e.is_regular_file() || e.path().extension() != ".fbx") continue;
		std::string name = e.path().stem().string();
		if (name == "X Bot") continue;
		m_motionList.push_back({ name,e.path().generic_string() });
	}
	std::sort(m_motionList.begin(), m_motionList.end(), [](const MotionEntry& a, const MotionEntry& b) {return a.name < b.name; });

}

// 名前からモーションを取得。未ロードなら一覧のパスから読み込む
aiAnimation* FightScene::ensureMotionLoaded(const std::string& name)
{
	if (!m_motions.Contains(name))
	{
		auto it = std::find_if(m_motionList.begin(), m_motionList.end(),
			[&](const MotionEntry& m) { return m.name == name; });
		if (it == m_motionList.end()) return nullptr;
		if (!m_motions.LoadAnimation(it->path, name)) return nullptr;
	}
	return m_motions.GetAnimation(name);
}

// 総キー数（全チャンネルの最大キー数）→ 終了判定に使う
int FightScene::countFrames(const aiAnimation* a)
{
	int n = 0;
	for (unsigned int c = 0; c < a->mNumChannels; ++c)
		n = std::max(n, (int)a->mChannels[c]->mNumRotationKeys);
	return n;
}

// 先頭から再生を始める。loop=false なら終了後に構えへ戻る
bool FightScene::playMotion(const std::string& name, bool loop,
	int beginFrame, int endFrame, float speedScale)
{
	aiAnimation* a = ensureMotionLoaded(name);
	if (!a || !m_p1AnimMesh) return false;

	m_p1Current = a;
	m_p1CurrentName = name;
	m_p1CurrentFrames = countFrames(a);

	// 再生範囲を決める（endFrame が負なら末尾まで）
	const int last = std::max(m_p1CurrentFrames - 1, 0);
	m_p1FrameBegin = std::clamp(beginFrame, 0, last);
	m_p1FrameEnd   = (endFrame < 0) ? last : std::clamp(endFrame, m_p1FrameBegin, last);
	m_p1SpeedScale = std::max(speedScale, 0.01f);

	m_p1Loop = loop;
	m_p1AnimFrame = (float)m_p1FrameBegin;
	m_p1RootValid = false;	// モーションを切り替えた瞬間に飛ばないよう、差分を取り直す
	m_p1AnimMesh->SetCurentAnimation(a);
	return true;
}

// ボーン2点を囲む長方形を1つ描く。2D格闘ゲームと同じく、判定は戦う平面（XY）だけで考える。
// 奥行き方向の位置と厚みは捨てて planeZ 上に平らに置くので、カメラを回しても形が変わらない。
void FightScene::drawBoneBox(const Vector3& a, const Vector3& b, float pad, float planeZ, const Color& col)
{
	if (!m_hitBox) return;

	// 板として見えるだけの最低限の厚み。判定には使わない
	constexpr float planeHalfZ = 1.0f;

	const Vector3 p(pad, pad, 0.0f);
	const Vector3 mn = Vector3::Min(a, b) - p;
	const Vector3 mx = Vector3::Max(a, b) + p;

	m_hitBox->SetSize(mx.x - mn.x, mx.y - mn.y, planeHalfZ * 2.0f);
	m_hitBox->Draw(Matrix4x4::CreateTranslation(
		Vector3((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, planeZ)), col);
}

// やられ判定（段階3：形の確認用。当たり判定はまだ行わない）
void FightScene::drawHurtboxes(const CAnimationMeshBlender* mesh,
	const Matrix4x4& world, float scale, const Color& col)
{
	if (!mesh || !m_hitBox) return;

	// 体幹・頭・両腕・両脚を覆う箱。形はモーション編集で変えられる（HurtboxConfig.json）
	const float planeZ = world.Translation().z;	// このキャラが戦っている平面
	const float facing = Hurtbox::FacingOf(world);	// 向きで「前方へのずれ」の向きが変わる

	for (const HurtboxDef& d : Hurtbox::Defs())
	{
		Vector3 a, b;
		if (!mesh->GetBonePosition(d.from, a)) continue;
		if (!mesh->GetBonePosition(d.to,   b)) continue;

		// モデル空間 → ワールド（描画と同じ行列を通す）
		const HurtRect r = Hurtbox::Compute(d, Vector3::Transform(a, world), Vector3::Transform(b, world), scale, facing);

		m_hitBox->SetSize(std::max(r.w(), 0.1f), std::max(r.h(), 0.1f), 2.0f);	// 厚みは板として見えるだけ
		m_hitBox->Draw(Matrix4x4::CreateTranslation(Vector3(r.cx(), r.cy(), planeZ)), col);
	}
}

// 押し合い判定。キャラの足元を基準にした縦長の箱を1個だけ置く。
// やられ判定と違ってアニメに追従させない（技を出すたびに押し合いが変わると挙動が不安定になるため）
void FightScene::drawPushbox(const CAnimationMeshBlender* mesh,
	const Matrix4x4& world, float scale, const Color& col)
{
	if (!mesh) return;

	Vector3 head;
	if (!mesh->GetBonePosition("mixamorig:Head", head)) return;
	head = Vector3::Transform(head, world);

	const Vector3 base = world.Translation();	// 原点が足元なのでそのまま地面の高さ
	const float   half = 26.0f * scale;			// 横幅の半分
	const float   top  = base.y + (head.y - base.y) + 14.0f * scale;

	drawBoneBox(Vector3(base.x, base.y, base.z), Vector3(base.x, top, base.z),
		half, base.z, col);
}

// プレイヤー2 のモーションを先頭から再生する（常にループ）
bool FightScene::playMotionP2(const std::string& name)
{
	aiAnimation* a = ensureMotionLoaded(name);	// キャッシュは P1 と共有
	if (!a || !m_p2AnimMesh) return false;

	m_p2Current       = a;
	m_p2CurrentFrames = countFrames(a);
	m_p2AnimFrame     = 0.0f;
	m_p2AnimMesh->SetCurentAnimation(a);
	return true;
}

// 「モデル」タブ内：Player1 のモーション選択（Begin/End は呼ばない）
void FightScene::drawMotionUI()
{
	ImGui::SeparatorText("プレイヤー1 モーション###P1 Motion");

	if (m_motionList.empty())
	{
		ImGui::TextDisabled("assets/motion に FBX がありません");
		return;
	}
	m_motionSelect = std::clamp(m_motionSelect, 0, (int)m_motionList.size() - 1);

	// 一覧から選ぶ（未ロードのものは印を付ける）
	if (ImGui::BeginCombo("モーション##motion", m_motionList[m_motionSelect].name.c_str()))
	{
		for (int i = 0; i < (int)m_motionList.size(); ++i)
		{
			const bool loaded = m_motions.Contains(m_motionList[i].name);
			std::string label = m_motionList[i].name + (loaded ? "" : "  (未読込)");
			if (ImGui::Selectable(label.c_str(), i == m_motionSelect)) m_motionSelect = i;
			if (i == m_motionSelect) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	const std::string selName = m_motionList[m_motionSelect].name;

	ImGui::Checkbox("ループ##motion loop", &m_motionSelectLoop);
	ImGui::SameLine();
	if (ImGui::Button("再生##play motion")) playMotion(selName, m_motionSelectLoop);

	// 選んだモーションを役割に割り当てる
	if (ImGui::Button("構えに設定##set idle")) { m_p1IdleName = selName; playMotion(m_p1IdleName, true); }
	ImGui::SameLine();
	if (ImGui::Button("技（J キー）に設定##set attack")) m_p1AttackName = selName;

	// 歩行の確認用。ルート移動は止めてあるのでその場で足踏みする
	ImGui::SeparatorText("歩行の確認###Walk check");
	if (ImGui::Button("前進##walk fwd"))  playMotion("sword and shield walk", true);
	ImGui::SameLine();
	if (ImGui::Button("後退##walk back")) playMotion("sword and shield walk (2)", true);
	ImGui::SameLine();
	if (ImGui::Button("構えへ戻す##back to idle")) playMotion(m_p1IdleName, true);

	// ステップ：歩行・走行のループサイクルの先頭だけを切り出して1回だけ再生する
	// 再生範囲を狭めるほど歩幅が小さくなる
	ImGui::SeparatorText("ステップ###Step");
	if (ImGui::Button("前ステップ##step fwd"))
		playMotion("sword and shield walk", false,
			m_stepFwdRange[0], m_stepFwdRange[1], m_stepFwdSpeed);
	ImGui::SameLine();
	if (ImGui::Button("バックステップ##step back"))
		playMotion("sword and shield run (2)", false,
			m_stepBackRange[0], m_stepBackRange[1], m_stepBackSpeed);

	ImGui::DragInt2("前 範囲（歩幅）##step fwd range", m_stepFwdRange, 0.2f, 0, 65);
	ImGui::SliderFloat("前 速度##step fwd speed", &m_stepFwdSpeed, 0.5f, 4.0f, "%.1f 倍");
	ImGui::DragInt2("後 範囲（歩幅）##step back range", m_stepBackRange, 0.2f, 0, 32);
	ImGui::SliderFloat("後 速度##step back speed", &m_stepBackSpeed, 0.5f, 4.0f, "%.1f 倍");

	// ルートモーションによる移動（アニメの歩幅ぶんだけ実際に動く）
	ImGui::SeparatorText("移動###Locomotion");
	ImGui::Checkbox("アニメの歩幅で移動する##apply root motion", &m_p1ApplyRootMotion);
	ImGui::SameLine();
	if (ImGui::Button("位置を戻す##reset pos")) { m_p1Pos = Vector3(-150, -100, 0); m_p1RootValid = false; }
	ImGui::Text("プレイヤー1 の位置: (%.0f, %.0f, %.0f)", m_p1Pos.x, m_p1Pos.y, m_p1Pos.z);

	ImGui::Text("構え: %s", m_p1IdleName.c_str());
	ImGui::Text("技　: %s", m_p1AttackName.c_str());

	ImGui::SliderFloat("再生速度（キー/秒）##anim fps", &m_animFps, 1.0f, 60.0f);

	if (m_p1Current)
		ImGui::Text("再生中: %s  %d （%d〜%d / 全%d）  %.1f倍  %s",
			m_p1CurrentName.c_str(), (int)m_p1AnimFrame,
			m_p1FrameBegin, m_p1FrameEnd, m_p1CurrentFrames,
			m_p1SpeedScale, m_p1Loop ? "(ループ)" : "(1回)");
}

// フォルダを走査して一覧のキャッシュを更新する。
// 毎フレーム走査すると無駄なので、選択が変わった時とボタンを押した時だけ呼ぶ。
void FightScene::refreshLibraryLists()
{
	m_libChars = CameraLibrary::Characters();
	m_libMoves = CameraLibrary::Moves(m_libChar);
	m_libTakes = CameraLibrary::Takes(m_libChar, m_libMove);
}

bool FightScene::applyModelByName(const std::string& fileName, int& selectIndex, std::string& meshId)
{
	if (fileName.empty()) return false;

	for (int i = 0; i < (int)g_loadmodel.size(); ++i)
	{
		if (getfilename(g_loadmodel[i].filename) != fileName) continue;

		selectIndex = i;
		meshId = ensureModelLoaded(i);
		return true;
	}
	return false;	// カタログから消えたモデル。呼び出し側で警告を出す
}


// 「ライブラリ」タブの中身。Begin/End は呼ばない。
void FightScene::drawLibraryUI()
{
	// 選んだテキストをバッファへ書き戻す小道具
	auto pickInto = [](const char* id, char* buf, size_t size,
		const std::vector<std::string>& list) -> bool
		{
			bool changed = false;
			if (ImGui::BeginCombo(id, buf))
			{
				for (const std::string& s : list)
				{
					if (ImGui::Selectable(s.c_str(), s == buf))
					{
						std::snprintf(buf, size, "%s", s.c_str());
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			return changed;
	};
	ImGui::SeparatorText("キャラクター##Character");

	bool dirty = false;
	dirty |= pickInto("既存から選ぶ##Pick character", m_libChar, sizeof(m_libChar), m_libChars);
	dirty |= ImGui::InputText("キャラ名##Character name", m_libChar, sizeof(m_libChar));

	// ---------------- 技 ----------------
	ImGui::SeparatorText("技##Move");

	dirty |= pickInto("既存から選ぶ##Pick move", m_libMove, sizeof(m_libMove), m_libMoves);
	dirty |= ImGui::InputText("技名##Move name", m_libMove, sizeof(m_libMove));

	// ---------------- テイク ----------------
	ImGui::SeparatorText("テイク##Take");

	pickInto("既存から選ぶ##Pick take", m_libTake, sizeof(m_libTake), m_libTakes);
	ImGui::InputText("テイク名##Take name", m_libTake, sizeof(m_libTake));

	// キャラや技が変わったら、下位の一覧を追従させる
	if (dirty) refreshLibraryLists();

	// ---------------- 保存される内容 ----------------
	ImGui::SeparatorText("保存される内容##Contents");

	ImGui::Text("制御点: %d 個", m_splineCam.PointCount());
	ImGui::Text("モデル: P1 = %s", m_meshid.empty() ? "（なし）" : m_meshid.c_str());
	ImGui::Text("　　　  P2 = %s", m_meshid2.empty() ? "（なし）" : m_meshid2.c_str());
	ImGui::Text("基準位置: (%.0f, %.0f, %.0f)", m_p1Pos.x, m_p1Pos.y, m_p1Pos.z);
	ImGui::TextDisabled("経路はこの基準位置からの相対座標で保存されます。");
	ImGui::TextDisabled("キャラがステージのどこにいても同じ画が撮れます。");

	// ---------------- 操作 ----------------
	ImGui::Separator();

	if (ImGui::Button("保存##Save take"))
	{
		CameraTake take;
		m_splineCam.CaptureTo(take, m_p1Pos);	// 絶対 → 相対はここで行われる

		take.character = m_libChar;
		take.move = m_libMove;
		take.name = m_libTake;
		take.p1Model = m_meshid;
		take.p2Model = m_meshid2;

		std::string err;
		if (CameraLibrary::Save(take, &err))
		{
			m_libStatus = std::string("保存しました: ")
				+ m_libChar + " / " + m_libMove + " / " + m_libTake + ".json";
			refreshLibraryLists();
		}
		else
		{
			m_libStatus = "保存できません: " + err;
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("読み込み##Load take"))
	{
		CameraTake take;
		std::string err;

		if (!CameraLibrary::Load(m_libChar, m_libMove, m_libTake, take, &err))
		{
			m_libStatus = "読み込めません: " + err;
		}
		else if (!m_splineCam.RestoreFrom(take, m_p1Pos))	// 相対 → 絶対
		{
			m_libStatus = "制御点が4点未満のため復元できません";
		}
		else
		{
			// 保存時のモデルに戻す。見つからなくても読み込み自体は成功扱いにする
			std::string warn;
			if (!take.p1Model.empty() && !applyModelByName(take.p1Model, m_p1Select, m_meshid))
				warn += "（P1モデル " + take.p1Model + " が見つかりません）";
			if (!take.p2Model.empty() && !applyModelByName(take.p2Model, m_p2Select, m_meshid2))
				warn += "（P2モデル " + take.p2Model + " が見つかりません）";

			// すぐ確認できるよう曲線カメラへ切り替える
			m_camMode = CamMode::Spline;
			m_splineCam.Play();			// 読み込んだ経路を先頭から流す

			m_libStatus = "読み込みました。" + warn;
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("削除##Delete take"))
	{
		if (CameraLibrary::Remove(m_libChar, m_libMove, m_libTake))
		{
			m_libStatus = std::string("削除しました: ") + m_libTake + ".json";
			refreshLibraryLists();
		}
		else
		{
			m_libStatus = "削除できません（ファイルがありません）";
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("一覧を更新##Refresh")) refreshLibraryLists();

	// ---------------- 状態 ----------------
	if (!m_libStatus.empty())
	{
		ImGui::Separator();
		ImGui::TextWrapped("%s", m_libStatus.c_str());
	}
}