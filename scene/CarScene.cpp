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

	std::string getfilename(std::string_view filestring) {
		auto u8name = std::filesystem::path(filestring).filename().u8string();
		return { reinterpret_cast<const char*>(u8name.data()), u8name.size() };
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
	ImGui::Begin(title);

	// 現在選択されているモデルの名前をプレビュー用に取得（範囲外アクセスも防止）
	std::string preview_name = "None";
	if (selectedIndex >= 0 && selectedIndex < g_loadmodel.size())
	{
		preview_name = getfilename(g_loadmodel[selectedIndex].filename);
	}

	// BeginComboを使ってドロップダウンを作成
	if (ImGui::BeginCombo("Model", preview_name.c_str()))
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

	ImGui::Text("Selected Model: %d", selectedIndex);

	ImGui::End();
}

// 角度から姿勢行列をつくる
void CarScene::debugRubikCubeRotation()
{

	ImGui::Begin("DebugRubikCube Rotation");

	ImGui::SliderFloat("X Rotation", &m_Rotation.x, 0.0f, PI);
	ImGui::SliderFloat("Y Rotation", &m_Rotation.y, 0.0f, PI);
	ImGui::SliderFloat("Z Rotation", &m_Rotation.z, 0.0f, PI);

	// 回転角度から回転行列を作成
	Matrix4x4 rotmtxX = Matrix4x4::CreateRotationX(m_Rotation.x);
	Matrix4x4 rotmtxY = Matrix4x4::CreateRotationY(m_Rotation.y);
	Matrix4x4 rotmtxZ = Matrix4x4::CreateRotationZ(m_Rotation.z);

	// 合成
	m_RotationMtx = rotmtxX * rotmtxY * rotmtxZ;

	// カメラの位置を極座標からデカルト座標に変換
	ImGui::End();
}

// ローカル軸回転
void CarScene::debugRubikCubeLocalRotation()
{
	Vector3 inputangle = { 0.0f,0.0f,0.0f };

	ImGui::Begin("DebugRubikCube Local Rotation");

	ImGui::SliderFloat("X Rotation", &inputangle.x, 0.0f, PI);
	ImGui::SliderFloat("Y Rotation", &inputangle.y, 0.0f, PI);
	ImGui::SliderFloat("Z Rotation", &inputangle.z, 0.0f, PI);

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

	ImGui::SliderFloat("X scale", &scale.x, 0.5f, 20.0f);
	ImGui::SliderFloat("Y scale", &scale.y, 0.5f, 20.0f);
	ImGui::SliderFloat("Z scale", &scale.z, 0.5f, 20.0f);

	m_ScaleMtx = Matrix4x4::CreateScale(scale);

	ImGui::End();
}

CarScene::CarScene()
{

}

// 既定の制御点をセット（原点を囲むXZ平面・高さ150・半径400の4点）
void CarScene::resetSplineDefault()
{
	m_splinePoints = {
		Vector3( 400, 150,    0), Vector3(   0, 150,  400),
		Vector3(-400, 150,    0), Vector3(   0, 150, -400)
	};
	m_splineT = 0.0f;
}

// u（連続パラメータ）位置のカメラ座標を Catmull-Rom で評価
Vector3 CarScene::evalSpline(float u) const
{
	int n = (int)m_splinePoints.size();
	if (n < 4) return m_camPos;

	int   seg = (int)floorf(u);
	float t   = u - seg;

	auto idx = [&](int k) -> int {
		if (m_splineLoop) return ((k % n) + n) % n;	// 閉曲線：巡回
		return std::clamp(k, 0, n - 1);				// 開曲線：端をクランプ
	};

	const Vector3& p0 = m_splinePoints[idx(seg - 1)];
	const Vector3& p1 = m_splinePoints[idx(seg)];
	const Vector3& p2 = m_splinePoints[idx(seg + 1)];
	const Vector3& p3 = m_splinePoints[idx(seg + 2)];

	return Vector3::CatmullRom(p0, p1, p2, p3, t);
}

// スプラインカメラのデバッグUI
void CarScene::debugSplineCamera()
{
	ImGui::Begin("Spline Camera");

	ImGui::Checkbox("Active (C key)", &m_splineActive);
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_splineLoop);
	ImGui::SameLine();
	ImGui::Checkbox("Show Path", &m_showSplinePath);

	ImGui::SliderFloat("Speed", &m_splineSpeed, 0.05f, 3.0f);
	ImGui::Text("t = %.2f", m_splineT);

	ImGui::DragFloat3("Lookat", &m_splineLookat.x, 1.0f);

	ImGui::Separator();
	ImGui::Text("Control Points (%d)", (int)m_splinePoints.size());

	for (int i = 0; i < (int)m_splinePoints.size(); ++i)
	{
		std::string label = "P" + std::to_string(i);
		ImGui::DragFloat3(label.c_str(), &m_splinePoints[i].x, 1.0f);
	}

	if (ImGui::Button("Add")) {
		// 末尾に、最後の点の近くへ新しい点を追加
		Vector3 p = m_splinePoints.empty() ? Vector3(0, 150, 0) : m_splinePoints.back() + Vector3(50, 0, 50);
		m_splinePoints.push_back(p);
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove")) {
		// 最低4点は維持
		if (m_splinePoints.size() > 4) m_splinePoints.pop_back();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		resetSplineDefault();
	}

	ImGui::End();
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
		if (index >= 0 && index < (int)m_splinePoints.size()) return &m_splinePoints[index];
		return nullptr;
	}
	if (kind == PickKind::Lookat)
	{
		return &m_splineLookat;
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

	for (int i = 0; i < (int)m_splinePoints.size(); ++i)
	{
		Vector3 hit; float t;
		float dist = Collision::calcPointLineDist(m_splinePoints[i], raySeg, hit, t);
		if (dist <= 12.0f && t >= 0.0f && t <= 1.0f && t < bestT)
		{
			bestT = t;
			bestKind = PickKind::ControlPoint;
			bestIndex = i;
		}
	}
	{
		Vector3 hit; float t;
		float dist = Collision::calcPointLineDist(m_splineLookat, raySeg, hit, t);
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

	// --- C キーでスプラインカメラ ⇔ フリーカメラ を切替（再生開始/停止） ---
	if (in.CheckKeyBufferTrigger(DIK_C))
	{
		m_splineActive = !m_splineActive;
		if (!m_splineActive)
		{
			// フリーへ戻る時、現在の向きから yaw/pitch を引き継ぐ
			Vector3 f = m_splineLookat - m_camPos;
			f.Normalize();
			m_yaw   = atan2f(f.x, f.z);
			m_pitch = asinf(f.y);
		}
	}

	if (m_splineActive && m_splinePoints.size() >= 4)
	{
		// --- スプライン曲線に沿ってカメラを進める ---
		int   n    = (int)m_splinePoints.size();
		float uMax = m_splineLoop ? (float)n : (float)(n - 1);

		m_splineT += m_splineSpeed * dt;
		if (m_splineLoop)
		{
			while (m_splineT >= uMax) m_splineT -= uMax;	// ループ
		}
		else if (m_splineT > uMax)
		{
			m_splineT = uMax;			// 端で停止
			m_splineActive = false;
		}

		m_camPos = evalSpline(m_splineT);		// 位置は経路上
		m_camera.SetPosition(m_camPos);
		m_camera.SetLookat(m_splineLookat);		// 常に固定の注視点を見る
		return;
	}

	// ===== 以下、フリーフライ（WASD＋マウス右ドラッグ）=====

	// --- マウス右ドラッグで視点回転 ---
	// ImGuiがマウスを使っている間（UI操作中）は回転しない
	bool uiCapturingMouse = ImGui::GetIO().WantCaptureMouse;

	if (in.GetMouseRButtonCheck() && !uiCapturingMouse)
	{
		int curX = in.GetMousePosX();
		int curY = in.GetMousePosY();

		if (!m_dragging)
		{
			// 押し始めは基準座標をセット（飛び防止）
			m_prevMouseX = curX;
			m_prevMouseY = curY;
			m_dragging = true;
		}
		else
		{
			int dx = curX - m_prevMouseX;
			int dy = curY - m_prevMouseY;

			m_yaw   += dx * m_lookSpeed;
			m_pitch -= dy * m_lookSpeed;

			m_prevMouseX = curX;
			m_prevMouseY = curY;
		}
	}
	else
	{
		m_dragging = false;
	}

	// ピッチを真上・真下手前でクランプ（破綻防止）
	const float pitchLimit = PI / 2.0f - 0.01f;
	if (m_pitch >  pitchLimit) m_pitch =  pitchLimit;
	if (m_pitch < -pitchLimit) m_pitch = -pitchLimit;

	// --- 前方／右ベクトル算出（左手座標系、yaw=Y軸回り・pitch=X軸回り） ---
	Vector3 forward{
		cosf(m_pitch) * sinf(m_yaw),
		sinf(m_pitch),
		cosf(m_pitch) * cosf(m_yaw)
	};
	forward.Normalize();

	Vector3 worldUp{ 0, 1, 0 };
	Vector3 right = worldUp.Cross(forward);	// LH: up × forward = right
	right.Normalize();

	// --- WASDで移動、Q/E(Space)で上下移動 ---
	float speed = m_moveSpeed;
	if (in.CheckKeyBuffer(DIK_LSHIFT)) speed *= 3.0f;	// Shiftで増速
	float dist = speed * dt;

	if (in.CheckKeyBuffer(DIK_W)) m_camPos += forward * dist;
	if (in.CheckKeyBuffer(DIK_S)) m_camPos -= forward * dist;
	if (in.CheckKeyBuffer(DIK_D)) m_camPos += right * dist;
	if (in.CheckKeyBuffer(DIK_A)) m_camPos -= right * dist;
	if (in.CheckKeyBuffer(DIK_E) || in.CheckKeyBuffer(DIK_SPACE)) m_camPos += worldUp * dist;
	if (in.CheckKeyBuffer(DIK_Q)) m_camPos -= worldUp * dist;

	// --- カメラへ反映（draw()内のm_camera.Draw()が使用する） ---
	m_camera.SetPosition(m_camPos);
	m_camera.SetLookat(m_camPos + forward);
}

void CarScene::draw(uint64_t deltatime)
{
	m_camera.Draw();

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

	ShaderManager::Get<CShader>("Shader3D")->SetGPU();

	// player1 を描画（左・相手を向く）
	Matrix4x4 w1 = m_ScaleMtx * Matrix4x4::CreateRotationY(m_p1FaceY) * Matrix4x4::CreateTranslation(m_p1Pos);
	Renderer::SetWorldMatrix(&w1);
	if (auto* r1 = MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid))  r1->Draw();

	// player2 を描画（右・相手を向く）
	Matrix4x4 w2 = m_ScaleMtx * Matrix4x4::CreateRotationY(m_p2FaceY) * Matrix4x4::CreateTranslation(m_p2Pos);
	Renderer::SetWorldMatrix(&w2);
	if (auto* r2 = MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid2)) r2->Draw();

	// スプライン経路の可視化（黄色線）
	if (m_showSplinePath && m_splinePoints.size() >= 4) {
		int   n    = (int)m_splinePoints.size();
		float uMax = m_splineLoop ? (float)n : (float)(n - 1);
		int   samples = (int)(uMax * 24);
		Vector3 prev = evalSpline(0);
		SetLineWidth(2);
		for (int i = 1; i <= samples; ++i) {
			Vector3 cur = evalSpline(uMax * i / samples);
			Vector3 dir = cur - prev;
			LineDrawerDraw(dir.Length(), prev, dir, Color(1, 1, 0, 1));
			prev = cur;
		}

		// 現在のスプラインカメラ位置に赤い球体を表示
		if (m_splineMarker) {
			Vector3 p = evalSpline(m_splineT);
			Matrix4x4 m = Matrix4x4::CreateTranslation(p);
			m_splineMarker->Draw(m, Color(1, 0, 0, 1));
		}

		// 注視点に青い球体を表示
		if (m_lookatMarker) {
			Matrix4x4 m = Matrix4x4::CreateTranslation(m_splineLookat);
			m_lookatMarker->Draw(m, Color(0, 0.4f, 1, 1));
		}

		// 各制御点に緑の球体を表示
		if (m_pointMarker) {
			for (const Vector3& cp : m_splinePoints) {
				Matrix4x4 m = Matrix4x4::CreateTranslation(cp);
				m_pointMarker->Draw(m, Color(0, 1, 0, 1));
			}
		}
	}

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

void CarScene::init()
{
	// カメラ(3D)の初期化
	m_camera.Init();
	m_camera.SetPosition(Vector3(0, 0, -300));
	m_camera.SetLookat(Vector3(0, 0, 0));
	m_camera.SetUP(Vector3(0, 1, 0));


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

	// 板ポリ（草の地面）を生成：大きめサイズ＋UVを繰り返してタイリング
	const float tile = 10.0f;	// 草を10×10回繰り返す
	std::array<Vector2, 4> grounduv = {
		Vector2(0, 0), Vector2(tile, 0), Vector2(0, tile), Vector2(tile, tile)
	};
	m_sprite = std::make_unique<CSprite>(2000, 2000, "assets/texture/Grass01.jpg", grounduv);


	// クオータニオンから回転行列
	DebugUI::RedistDebugFunction([this]() {
		debugRubikCubeLocalRotation();
		});

	// モデル選択（player1 / player2 を個別に選べる）
	DebugUI::RedistDebugFunction([this]() {
		debugModelSelect("Player1 Model", m_p1Select, m_meshid);
		});
	DebugUI::RedistDebugFunction([this]() {
		debugModelSelect("Player2 Model", m_p2Select, m_meshid2);
		});

	// スプラインカメラ：既定の制御点をセットし、編集UIを登録
	resetSplineDefault();
	m_splineMarker = std::make_unique<Sphere>(15.0f);	// 現在地点マーカー（赤）
	m_lookatMarker = std::make_unique<Sphere>(15.0f);	// 注視点マーカー（青）
	m_pointMarker  = std::make_unique<Sphere>(12.0f);	// 制御点マーカー（緑）

	// 軸ギズモ（矢印＝円柱の軸＋円錐の先端）
	m_gizmoShaft = std::make_unique<Cylinder>(m_gizmoShaftRad, m_gizmoShaftLen);
	m_gizmoHead  = std::make_unique<Cone>(m_gizmoHeadRad, m_gizmoHeadLen);
	DebugUI::RedistDebugFunction([this]() {
		debugSplineCamera();
		});


}


void CarScene::dispose()
{

}