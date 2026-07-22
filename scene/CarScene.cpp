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

	std::array<Load3DInfo, 16> g_loadmodel =
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
				"assets/motion/"),			// テクスチャのパス

			Load3DInfo(
				"assets/model/o1kaensO5D/suzu.pmx",	// モデル名（铃：本体キャラ）
				"assets/model/o1kaensO5D/")			// テクスチャのパス
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

	// Player1(suzu/PMX) 専用スケール（Player2やX Botには影響しない）
	ImGui::SliderFloat("P1(suzu) scale", &m_p1Scale, 1.0f, 30.0f);

	ImGui::End();
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

	// --- C キーでスプラインカメラ ⇔ フリーカメラ を切替（再生開始/停止） ---
	if (in.CheckKeyBufferTrigger(DIK_C))
	{
		bool now = !m_splineCam.IsActive();
		m_splineCam.SetActive(now);
		if (!now)
		{
			// フリーへ戻る時、現在の位置・向きをフリーカメラへ引き継ぐ
			m_freeCam.AdoptFromLookAt(m_splineCam.CurrentPos(), m_splineCam.Lookat());
		}
	}

	// --- カメラ更新（アクティブな方が m_camera に反映する） ---
	if (m_splineCam.IsActive())
	{
		m_splineCam.Update(dt, m_camera);
	}
	else
	{
		m_freeCam.Update(dt, m_camera);
	}
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

void CarScene::init()
{
	// カメラ(3D)の初期化
	m_camera.Init();
	m_camera.SetPosition(Vector3(0, 0, -300));
	m_camera.SetLookat(Vector3(0, 0, 0));
	m_camera.SetUP(Vector3(0, 1, 0));

	// フリーカメラの初期姿勢（従来の固定カメラと同じ見え方）
	m_freeCam.AdoptFromLookAt(Vector3(0, 0, -300), Vector3(0, 0, 0));


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

	// スプラインカメラ：既定の制御点・マーカーを生成し、編集UIを登録
	m_splineCam.Init();
	DebugUI::RedistDebugFunction([this]() {
		m_splineCam.DebugUI();
		});

	// 軸ギズモ（矢印＝円柱の軸＋円錐の先端）
	m_gizmoShaft = std::make_unique<Cylinder>(m_gizmoShaftRad, m_gizmoShaftLen);
	m_gizmoHead  = std::make_unique<Cone>(m_gizmoHeadRad, m_gizmoHeadLen);


}


void CarScene::dispose()
{

}

