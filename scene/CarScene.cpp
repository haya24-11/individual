#include    <memory>
#include	<string>
#include	<array>
#include	<vector>
#include	<cstdint>
#include	<fstream>
#include	"CarScene.h"
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
std::string CarScene::ensureModelLoaded(int index)
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
void CarScene::debugModelSelect(const char* title, int& selectedIndex, std::string& meshId)
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
void CarScene::drawMainToolsUI()
{
	ImGui::SetNextWindowSize(ImVec2(520, 720), ImGuiCond_FirstUseEver);
	ImGui::Begin("カメラワーク ツール###Main Tools");

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
				m_splineCam.SetActive(true);
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

		// ---------------- 録画 ----------------
		if (ImGui::BeginTabItem("録画##Recorder"))
		{
			m_recorder.DrawUI();
			ImGui::EndTabItem();
		}

		// ---------------- モデル ----------------
		if (ImGui::BeginTabItem("モデル##Model"))
		{
			debugModelSelect("プレイヤー1 モデル###Player1 Model", m_p1Select, m_meshid);
			debugModelSelect("プレイヤー2 モデル###Player2 Model", m_p2Select, m_meshid2);
			ImGui::EndTabItem();
		}

		// ---------------- デバッグ ----------------
		if (ImGui::BeginTabItem("デバッグ##Debug"))
		{
			debugRubikCubeLocalRotation();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// ローカル軸回転
void CarScene::debugRubikCubeLocalRotation()
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

CarScene::CarScene()
{

}

// マウス位置からワールド空間のレイ（始点・方向）を計算する
bool CarScene::computeMouseRay(Vector3& outOrigin, Vector3& outDir) const
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
Vector3* CarScene::getPickTarget(PickKind kind, int index)
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
void CarScene::updateGizmoPicking()
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
void CarScene::drawGizmo()
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

void CarScene::update(uint64_t deltatime)
{
	// マイクロ秒 → 秒
	float dt = deltatime / 1'000'000.0f;

	CDirectInput& in = CDirectInput::GetInstance();

	// 制御点・注視点のギズモ操作（スプラインカメラの再生状態に関わらず動作させる）
	updateGizmoPicking();

	// --- Player1 アッパーカットモーション（J キーで再生開始、終了で構えに戻る） ---
	if (in.CheckKeyBufferTrigger(DIK_J) && !m_p1Attacking)
	{
		m_p1Attacking = true;
		m_p1AnimFrame = 0.0f;
	}
	if (m_p1Attacking)
	{
		m_p1AnimFrame += m_animFps * dt;
		if (m_p1AnimFrame >= (float)(m_uppercutFrames - 1))
		{
			// 最終キーまで再生したら終了して構え（先頭フレーム）へ戻す
			m_p1Attacking = false;
			m_p1AnimFrame = 0.0f;
		}
	}
	// 姿勢を毎フレーム計算（非再生時はフレーム0の構えを維持）
	if (m_p1AnimMesh)
	{
		int frame = (int)m_p1AnimFrame;
		m_p1AnimMesh->Update(m_p1BoneComb, frame);
	}

	// --- C キー: メインカメラを 基本 ⇔ スプライン でトグル ---
	if (in.CheckKeyBufferTrigger(DIK_C))
	{
		m_camMode = (m_camMode == CamMode::Spline) ? CamMode::Fighting : CamMode::Spline;
		m_splineCam.SetActive(m_camMode == CamMode::Spline);	// スプライン時のみ再生
		if (m_camMode == CamMode::Fighting) m_fightCam.ResetSnap();	// 復帰時は即スナップ
	}

	// デバッグ第2ビューのカメラ操作はビュー窓のImGuiラムダ内で行う（別OSウィンドウでも効くように）

	// --- メインカメラを m_camera に反映する ---
	if (m_recorder.IsPlaying())
	{
		m_recorder.ApplyPlayback(m_camera, dt);				// 録画テイクで上書き（最優先）
	}
	else if (m_manualCam)
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

	// 録画中（再生中を除く）は、確定した姿勢を1フレーム分記録
	if (m_recorder.IsRecording() && !m_recorder.IsPlaying())
		m_recorder.Sample(m_camera, dt);
}

void CarScene::draw(uint64_t deltatime)
{
	// メインパス（従来どおり main RT へ）
	m_camera.Draw();
	drawSceneGeometry();

	// デバッグ第2ビュー（開いている時だけ free-fly 視点でオフスクリーンへ再描画）
	if (m_debugViewOpen && m_dbgRTV) renderDebugView();
}

// シーンの3D描画本体（メイン／第2ビュー共通。view/projは呼び出し前に設定済み前提）
void CarScene::drawSceneGeometry()
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
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	if (auto* r1 = MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid)) r1->Draw();

	// player2 を描画（右・相手を向く）: 従来どおり静的描画（シェーダを戻す）
	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	Matrix4x4 w2 = m_ScaleMtx * Matrix4x4::CreateRotationY(m_p2FaceY) * Matrix4x4::CreateTranslation(m_p2Pos);
	Renderer::SetWorldMatrix(&w2);
	if (auto* r2 = MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid2)) r2->Draw();

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

}

// デバッグ第2ビュー用のオフスクリーンRT（カラー＋深度）を生成
void CarScene::createDebugTarget()
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
void CarScene::renderDebugView()
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

void CarScene::init()
{
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

	// アッパーカットモーションを別FBXから読み込み、モデルに結合
	m_uppercutData = std::make_unique<CAnimationData>();
	m_uppercutData->LoadAnimation("assets/motion/Uppercut Jab.fbx", "uppercut");
	aiAnimation* upper = m_uppercutData->GetAnimation("uppercut", 0);
	m_p1AnimMesh->SetCurentAnimation(upper);

	// 総キー数（全チャンネルの最大キー数）→ ワンショット終了判定に使用
	for (unsigned int c = 0; c < upper->mNumChannels; ++c) {
		m_uppercutFrames = std::max(m_uppercutFrames, (int)upper->mChannels[c]->mNumRotationKeys);
	}

	// ボーン行列定数バッファ(b5)を生成
	m_p1BoneComb.Create();

	// 板ポリ（草の地面）を生成：大きめサイズ＋UVを繰り返してタイリング
	const float tile = 10.0f;	// 草を10×10回繰り返す
	std::array<Vector2, 4> grounduv = {
		Vector2(0, 0), Vector2(tile, 0), Vector2(0, tile), Vector2(tile, tile)
	};
	m_sprite = std::make_unique<CSprite>(2000, 2000, "assets/texture/Grass01.jpg", grounduv);


	// スプラインカメラ：既定の制御点・マーカーを生成
	m_splineCam.Init();

	// 全機能を1つのウィンドウにまとめて登録する（タブ＋折りたたみ）
	DebugUI::RedistDebugFunction([this]() {
		drawMainToolsUI();
		});

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
		});

	// 軸ギズモ（矢印＝円柱の軸＋円錐の先端）
	m_gizmoShaft = std::make_unique<Cylinder>(m_gizmoShaftRad, m_gizmoShaftLen);
	m_gizmoHead  = std::make_unique<Cone>(m_gizmoHeadRad, m_gizmoHeadLen);


}


void CarScene::dispose()
{

}
