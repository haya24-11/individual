#include "CameraRecorder.h"
#include "../system/imgui/imgui.h"
#include <algorithm>
#include <fstream>

// 録画中：現在のカメラ姿勢を記録時刻付きで追加
void CameraRecorder::Sample(const Camera& cam, float dt)
{
	CamKey k;
	k.t    = m_recTime;
	k.pos  = cam.GetPosition();
	k.look = cam.GetLookat();
	k.up   = cam.GetUP();
	m_keys.push_back(k);
	m_recTime += dt;
}

// 再生中：m_playTime を進め、前後2キーを線形補間して cam に反映
void CameraRecorder::ApplyPlayback(Camera& cam, float dt)
{
	if (m_keys.empty()) { m_playing = false; return; }
	if (m_keys.size() == 1) {
		cam.SetPosition(m_keys[0].pos);
		cam.SetLookat(m_keys[0].look);
		cam.SetUP(m_keys[0].up);
		return;
	}

	m_playTime += dt;
	float dur = m_keys.back().t;

	if (m_playTime >= dur) {
		if (m_loop) { if (dur > 1e-6f) m_playTime = fmodf(m_playTime, dur); else m_playTime = 0.0f; }
		else        { m_playTime = dur; m_playing = false; }	// 末尾で停止（姿勢は末尾のまま）
	}

	// m_playTime を挟む区間 [j, j+1] を二分探索で見つける
	auto it = std::upper_bound(m_keys.begin(), m_keys.end(), m_playTime,
		[](float t, const CamKey& k) { return t < k.t; });
	int j = (int)(it - m_keys.begin()) - 1;
	j = std::clamp(j, 0, (int)m_keys.size() - 2);

	const CamKey& a = m_keys[j];
	const CamKey& b = m_keys[j + 1];
	float span = b.t - a.t;
	float f = (span > 1e-6f) ? (m_playTime - a.t) / span : 0.0f;

	Vector3 up = Vector3::Lerp(a.up, b.up, f);
	if (up.LengthSquared() > 1e-8f) up.Normalize();

	cam.SetPosition(Vector3::Lerp(a.pos,  b.pos,  f));
	cam.SetLookat(Vector3::Lerp(a.look, b.look, f));
	cam.SetUP(up);
}

void CameraRecorder::StartRecord()
{
	m_keys.clear();
	m_recTime   = 0.0f;
	m_recording = true;
	m_playing   = false;
}

void CameraRecorder::StartPlay()
{
	if (m_keys.size() < 2) return;
	m_playTime  = 0.0f;
	m_playing   = true;
	m_recording = false;
}

// テキスト保存：1行目 count、以降 t px py pz lx ly lz ux uy uz
void CameraRecorder::Save(const char* path)
{
	std::ofstream ofs(path);
	if (!ofs) return;
	ofs << m_keys.size() << "\n";
	for (const CamKey& k : m_keys) {
		ofs << k.t << " "
			<< k.pos.x  << " " << k.pos.y  << " " << k.pos.z  << " "
			<< k.look.x << " " << k.look.y << " " << k.look.z << " "
			<< k.up.x   << " " << k.up.y   << " " << k.up.z   << "\n";
	}
}

void CameraRecorder::Load(const char* path)
{
	std::ifstream ifs(path);
	if (!ifs) return;
	size_t n = 0;
	ifs >> n;
	std::vector<CamKey> loaded;
	loaded.reserve(n);
	for (size_t i = 0; i < n; ++i) {
		CamKey k;
		ifs >> k.t
			>> k.pos.x  >> k.pos.y  >> k.pos.z
			>> k.look.x >> k.look.y >> k.look.z
			>> k.up.x   >> k.up.y   >> k.up.z;
		if (!ifs) break;
		loaded.push_back(k);
	}
	if (!loaded.empty()) {
		m_keys = std::move(loaded);
		m_playing = m_recording = false;
		m_playTime = 0.0f;
	}
}

void CameraRecorder::DrawUI()
{
	float dur = m_keys.empty() ? 0.0f : m_keys.back().t;
	ImGui::Text("キー数: %d   長さ: %.2f 秒", (int)m_keys.size(), dur);

	// 録画ボタン（トグル）
	if (!m_recording) {
		if (ImGui::Button("録画##Record")) StartRecord();
	} else {
		if (ImGui::Button("録画停止##Stop Rec")) m_recording = false;
	}
	ImGui::SameLine();

	// 再生ボタン（トグル）
	if (!m_playing) {
		if (ImGui::Button("再生##Play")) StartPlay();
	} else {
		if (ImGui::Button("再生停止##Stop Play")) m_playing = false;
	}
	ImGui::SameLine();
	ImGui::Checkbox("ループ##Loop", &m_loop);

	if (ImGui::Button("クリア##Clear")) { m_keys.clear(); m_playing = m_recording = false; }
	ImGui::SameLine();
	if (ImGui::Button("保存##Save")) Save("camera_take.txt");
	ImGui::SameLine();
	if (ImGui::Button("読み込み##Load")) Load("camera_take.txt");

	if (m_playing && dur > 1e-6f) {
		ImGui::ProgressBar(m_playTime / dur, ImVec2(-1, 0));
	}

	ImGui::TextDisabled("録画: どのカメラ操作でも記録します。手動フリーカメラで手動録画できます。");
}
