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

	std::array<Load3DInfo, 14> g_loadmodel =
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
				"assets/model/jack1/")						// テクスチャのパス
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

// モデル選択
void CarScene::debugModelSelect()
{
	static int selected_model = 0;

	ImGui::Begin("Model Selector");

	// 現在選択されているモデルの名前をプレビュー用に取得（範囲外アクセスも防止）
	std::string preview_name = "None";
	if (selected_model >= 0 && selected_model < g_loadmodel.size())
	{
		preview_name = getfilename(g_loadmodel[selected_model].filename);
	}

	// BeginComboを使ってドロップダウンを作成
	if (ImGui::BeginCombo("Model", preview_name.c_str()))
	{
		for (int i = 0; i < g_loadmodel.size(); ++i)
		{
			const bool is_selected = (selected_model == i);
			std::string item_name = getfilename(g_loadmodel[i].filename);

			// リストの各アイテムを描画し、クリックされたか判定
			if (ImGui::Selectable(item_name.c_str(), is_selected))
			{
				selected_model = i;

				m_meshid = getfilename(g_loadmodel[selected_model].filename);

				if (MeshManager::ContainsRenderer(item_name)==false) {
					
					// メッシュを生成
					std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
					mesh->Load(g_loadmodel[selected_model].filename, g_loadmodel[selected_model].texdirectoryname);

					// メッシュレンダラを生成
					std::unique_ptr<CStaticMeshRenderer> meshrenderer = std::make_unique<CStaticMeshRenderer>();
					meshrenderer->Init(*mesh.get());

					MeshManager::RegisterMesh<CStaticMesh>(m_meshid, std::move(mesh));
					MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(m_meshid, std::move(meshrenderer));

				}
			}

			// ドロップダウンを開いた時、現在選択されているアイテムにフォーカスを合わせる
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Text("Selected Model: %d", selected_model);

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

void CarScene::update(uint64_t deltatime)
{
	CDirectInput& di = CDirectInput::GetInstance();

	// 現在のカメラ状態を取得
	Vector3 pos     = m_camera.GetPosition();
	Vector3 lookat  = m_camera.GetLookat();
	Vector3 worldup = m_camera.GetUP();			// {0,1,0}

	// カメラ基準の前方向・右方向を算出（左手座標系）
	Vector3 forward = lookat - pos;
	forward.Normalize();
	Vector3 right = worldup.Cross(forward);		// up × forward = right
	right.Normalize();

	// deltatime[ms] に応じた移動量
	constexpr float SPEED = 300.0f;				// 1秒あたりの移動距離（要調整）
	float move = SPEED * (static_cast<float>(deltatime) / 1000.0f);

	Vector3 delta(0, 0, 0);
	if (di.CheckKeyBuffer(DIK_W))    delta += forward * move;	// 前進
	if (di.CheckKeyBuffer(DIK_S))    delta -= forward * move;	// 後退
	if (di.CheckKeyBuffer(DIK_D))    delta += right   * move;	// 右
	if (di.CheckKeyBuffer(DIK_A))    delta -= right   * move;	// 左
	if (di.CheckKeyBuffer(DIK_UP))   delta += worldup * move;	// 上昇
	if (di.CheckKeyBuffer(DIK_DOWN)) delta -= worldup * move;	// 下降

	// 位置と注視点を同じだけ平行移動（視線方向は維持）
	pos    += delta;
	lookat += delta;
	m_camera.SetPosition(pos);
	m_camera.SetLookat(lookat);
}

void CarScene::draw(uint64_t deltatime)
{
	m_camera.Draw();

	// 地面（板ポリ）を描画：X軸90°で寝かせ、大きくスケール
	m_ground->Draw(
		Vector3(4000.0f, 4000.0f, 1.0f),		// scale：地面の広さ
		Vector3(-PI / 2.0f, 0.0f, 0.0f),		// rotation：X軸90°で水平に寝かせる
		Vector3(0.0f, -50.0f, 0.0f));			// pos：モデルの足元に配置（要調整）

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

	Matrix4x4 mtx = m_ScaleMtx* m_RotationMtx;
	Renderer::SetWorldMatrix(&mtx);

	ShaderManager::Get<CShader>("Shader3D")->SetGPU();
	MeshManager::getRenderer<CStaticMeshRenderer>(m_meshid)->Draw();

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

	// メッシュを生成
	std::unique_ptr<CStaticMesh> mesh{};
	mesh = std::make_unique<CStaticMesh>();
	mesh->Load(g_loadmodel[0].filename, g_loadmodel[0].texdirectoryname);

	// メッシュレンダラを生成
	std::unique_ptr<CStaticMeshRenderer> meshrenderer{};
	meshrenderer = std::make_unique<CStaticMeshRenderer>();
	meshrenderer->Init(*mesh.get());

	m_meshid = getfilename(g_loadmodel[0].filename);

	MeshManager::RegisterMesh<CStaticMesh>(m_meshid, std::move(mesh));
	MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(m_meshid, std::move(meshrenderer));

	// 地面（板ポリ）を生成。サイズはスケールで決めるので 1x1 で作る
	m_ground = std::make_unique<CSprite>(
		1, 1, "assets/texture/UI64x64.png");	// ダミーテクスチャ（後で無効化）

	// テクスチャを使わず単色にするマテリアルを設定
	MATERIAL groundmtrl{};
	groundmtrl.Diffuse  = Color(0.3f, 0.5f, 0.3f, 1.0f);	// 地面色（任意）
	groundmtrl.Ambient  = Color(0, 0, 0, 0);
	groundmtrl.Emission = Color(0, 0, 0, 0);
	groundmtrl.Specular = Color(0, 0, 0, 0);
	groundmtrl.TextureEnable = FALSE;						// テクスチャOFF＝単色
	m_ground->ModifyMtrl(groundmtrl);


	// クオータニオンから回転行列
	DebugUI::RedistDebugFunction([this]() {
		debugRubikCubeLocalRotation();
		});

	// モデル選択
	DebugUI::RedistDebugFunction([this]() {
		debugModelSelect();
		});


}


void CarScene::dispose()
{

}