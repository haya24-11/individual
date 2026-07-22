#include "SplineCamera.h"
#include "../system/imgui/imgui.h"
#include "../system/LineDrawer.h"
#include "../system/renderer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdio>

// 既定の制御点をセット（原点を囲むXZ平面・高さ150・半径400の4点）
void SplineCamera::ResetDefault()
{
	m_points = {
		Vector3( 400, 150,    0), Vector3(   0, 150,  400),
		Vector3(-400, 150,    0), Vector3(   0, 150, -400)
	};
	m_speeds.assign(m_points.size(), 1.0f);	// 速度倍率は全点1.0（等速）
	m_t = 0.0f;
	m_dist = 0.0f;
}

void SplineCamera::Init()
{
	ResetDefault();
	m_marker       = std::make_unique<Sphere>(15.0f);	// 現在地点マーカー（赤）
	m_lookatMarker = std::make_unique<Sphere>(15.0f);	// 注視点マーカー（青）
	m_pointMarker  = std::make_unique<Sphere>(12.0f);	// 制御点マーカー（緑）
}

// u（連続パラメータ）位置のカメラ座標を Catmull-Rom で評価
Vector3 SplineCamera::Eval(float u) const
{
	int n = (int)m_points.size();
	if (n < 4) return m_lookat;	// 点が足りないときは注視点を返す（退化ガード）

	int   seg = (int)floorf(u);
	float t   = u - seg;

	auto idx = [&](int k) -> int {
		if (m_loop) return ((k % n) + n) % n;	// 閉曲線：巡回
		return std::clamp(k, 0, n - 1);			// 開曲線：端をクランプ
	};

	const Vector3& p0 = m_points[idx(seg - 1)];
	const Vector3& p1 = m_points[idx(seg)];
	const Vector3& p2 = m_points[idx(seg + 1)];
	const Vector3& p3 = m_points[idx(seg + 2)];

	return Vector3::CatmullRom(p0, p1, p2, p3, t);
}

// u位置の実効速度倍率を求める（隣接4点をCatmull-Romで補間＝加速度も連続で繋ぎ目が分からない）
float SplineCamera::SpeedAt(float u) const
{
	int n = (int)m_points.size();
	if (n < 4 || m_speeds.size() != m_points.size()) return 1.0f;

	int   seg = (int)floorf(u);
	float t   = u - seg;

	auto idx = [&](int k) -> int {
		if (m_loop) return ((k % n) + n) % n;
		return std::clamp(k, 0, n - 1);
	};

	// 位置カーブ(Eval)と同じ4点ステンシル。スカラー速度を x に詰めてCatmull-Rom補間する。
	float s0 = m_speeds[idx(seg - 1)];
	float s1 = m_speeds[idx(seg)];
	float s2 = m_speeds[idx(seg + 1)];
	float s3 = m_speeds[idx(seg + 2)];

	Vector3 r = Vector3::CatmullRom(
		Vector3(s0, 0, 0), Vector3(s1, 0, 0), Vector3(s2, 0, 0), Vector3(s3, 0, 0), t);

	// Catmull-Romは近傍値をオーバーシュートし得る→0以下だと停止・逆走するのでUI下限に合わせてクランプ
	return std::max(r.x, 0.05f);
}

// 弧長テーブルを構築する
// 曲線を u 等間隔でサンプリングし、u=0 から各サンプルまでの「累積距離」を配列に記録する。
void SplineCamera::BuildArcTable()
{
	m_arc.clear();
	m_totalLen = 0.0f;

	int n = (int)m_points.size();
	if (n < 4) return;	// Catmull-Rom には最低4点必要

	int   segCount = m_loop ? n : (n - 1);
	float uMax     = (float)segCount;

	int M = m_samplesPerSeg * segCount;
	if (M < 1) return;

	m_arc.resize(M + 1);
	m_arc[0] = 0.0f;

	Vector3 prev = Eval(0.0f);
	for (int i = 1; i <= M; ++i)
	{
		float   u   = uMax * (float)i / (float)M;
		Vector3 cur = Eval(u);
		m_arc[i] = m_arc[i - 1] + (cur - prev).Length();
		prev = cur;
	}

	m_totalLen = m_arc[M];	// 最後の累積値 = 経路の総距離
}

// 距離 s（弧長）から曲線パラメータ u へ逆引きする
float SplineCamera::ArcLengthToU(float s) const
{
	int M = (int)m_arc.size() - 1;
	if (M < 1) return 0.0f;

	float uMax  = (float)M / (float)m_samplesPerSeg;	// BuildArcTable と一致
	float total = m_arc[M];
	if (total <= 1e-6f) return 0.0f;

	s = std::clamp(s, 0.0f, total);	// s を [0, 総距離] に収める

	// 累積距離配列は単調増加 → s を超える最初の要素の1つ前が区間の始点 j
	auto it = std::upper_bound(m_arc.begin(), m_arc.end(), s);
	int  j  = (int)(it - m_arc.begin()) - 1;
	j = std::clamp(j, 0, M - 1);

	float segLen = m_arc[j + 1] - m_arc[j];
	float frac   = (segLen > 1e-6f) ? (s - m_arc[j]) / segLen : 0.0f;

	float sampleIndex = (float)j + frac;
	return uMax * sampleIndex / (float)M;
}

void SplineCamera::Update(float dt, Camera& cam)
{
	if (!m_active || m_points.size() < 4) return;

	// 弧長テーブルを毎フレーム再構築（ギズモで制御点を編集中でも追従させるため）
	BuildArcTable();

	if (m_totalLen < 1e-4f)
	{
		// 退化ガード（全制御点がほぼ同一など）
		cam.SetPosition(Eval(m_t));
		cam.SetLookat(m_lookat);
		return;
	}

	// マスター速度[ワールド単位/秒] × 現在位置の速度倍率（前フレームの u で評価）
	float worldSpeed = m_speed * SpeedAt(m_t);
	m_dist += worldSpeed * dt;	// 進行を「距離」で加算

	if (m_loop)
	{
		while (m_dist >= m_totalLen) m_dist -= m_totalLen;	// 距離でループ
	}
	else if (m_dist > m_totalLen)
	{
		m_dist = m_totalLen;	// 端で停止
		m_active = false;
	}

	m_t = ArcLengthToU(m_dist);	// 距離 → u（弧長パラメータ化の核心）

	cam.SetPosition(Eval(m_t));		// 位置は経路上
	cam.SetLookat(m_lookat);		// 常に固定の注視点を見る
}

void SplineCamera::DrawVisualization()
{
	if (!m_showPath || m_points.size() < 4) return;

	int   n    = (int)m_points.size();
	float uMax = m_loop ? (float)n : (float)(n - 1);
	int   samples = (int)(uMax * 24);

	// 経路（黄色線）
	Vector3 prev = Eval(0);
	SetLineWidth(2);
	for (int i = 1; i <= samples; ++i) {
		Vector3 cur = Eval(uMax * i / samples);
		Vector3 dir = cur - prev;
		LineDrawerDraw(dir.Length(), prev, dir, Color(1, 1, 0, 1));
		prev = cur;
	}

	// 現在のカメラ位置（赤）
	if (m_marker) {
		Matrix4x4 m = Matrix4x4::CreateTranslation(Eval(m_t));
		m_marker->Draw(m, Color(1, 0, 0, 1));
	}

	// 注視点（青）
	if (m_lookatMarker) {
		Matrix4x4 m = Matrix4x4::CreateTranslation(m_lookat);
		m_lookatMarker->Draw(m, Color(0, 0.4f, 1, 1));
	}

	// 各制御点（緑）
	if (m_pointMarker) {
		for (const Vector3& cp : m_points) {
			Matrix4x4 m = Matrix4x4::CreateTranslation(cp);
			m_pointMarker->Draw(m, Color(0, 1, 0, 1));
		}
	}
}

// 平面(axisH, axisV) を1枚の2Dキャンバスとして描画・編集する
void SplineCamera::DrawEditCanvas(const char* id, int axisH, int axisV, bool allowAddRemove)
{
	m_speeds.resize(m_points.size(), 1.0f);	// サイズ同期（保険）

	// Vector3 から指定軸の成分を取り出す
	auto comp = [](const Vector3& v, int axis) -> float {
		return (axis == 0) ? v.x : (axis == 1) ? v.y : v.z;
	};
	// 制御点の指定軸成分へのポインタ
	auto compPtr = [](Vector3& v, int axis) -> float* {
		return (axis == 0) ? &v.x : (axis == 1) ? &v.y : &v.z;
	};

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	float avail = ImGui::GetContentRegionAvail().x;
	if (avail < 50.0f) avail = 50.0f;
	ImVec2 size(avail, 220.0f);

	ImGui::InvisibleButton(id, size);	// 領域確保＋マウス捕捉
	bool hovered = ImGui::IsItemHovered();

	const float half = m_editViewHalf;

	// ワールド↔スクリーン変換（縦軸は画面下向きに反転）
	auto worldToScreen = [&](float wh, float wv) -> ImVec2 {
		float sx = p0.x + (wh + half) / (2.0f * half) * size.x;
		float sy = p0.y + (1.0f - (wv + half) / (2.0f * half)) * size.y;
		return ImVec2(sx, sy);
	};
	auto screenToWorldH = [&](float sx) -> float {
		return ((sx - p0.x) / size.x) * (2.0f * half) - half;
	};
	auto screenToWorldV = [&](float sy) -> float {
		return (1.0f - (sy - p0.y) / size.y) * (2.0f * half) - half;
	};

	// 背景・枠・原点十字
	ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
	dl->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
	dl->AddRect(p0, p1, IM_COL32(90, 90, 100, 255));
	ImVec2 o = worldToScreen(0, 0);
	dl->AddLine(ImVec2(p0.x, o.y), ImVec2(p1.x, o.y), IM_COL32(60, 60, 70, 255));
	dl->AddLine(ImVec2(o.x, p0.y), ImVec2(o.x, p1.y), IM_COL32(60, 60, 70, 255));

	const char* axisName[3] = { "X", "Y", "Z" };
	dl->AddText(ImVec2(p1.x - 14, o.y - 14), IM_COL32(150, 150, 160, 255), axisName[axisH]);
	dl->AddText(ImVec2(o.x + 4, p0.y + 2), IM_COL32(150, 150, 160, 255), axisName[axisV]);

	// 曲線（黄）
	if (m_points.size() >= 4)
	{
		int   n    = (int)m_points.size();
		float uMax = m_loop ? (float)n : (float)(n - 1);
		int   samples = 128;
		Vector3 prev = Eval(0);
		for (int i = 1; i <= samples; ++i)
		{
			Vector3 cur = Eval(uMax * i / samples);
			ImVec2 a = worldToScreen(comp(prev, axisH), comp(prev, axisV));
			ImVec2 b = worldToScreen(comp(cur, axisH), comp(cur, axisV));
			dl->AddLine(a, b, IM_COL32(230, 220, 40, 255), 2.0f);
			prev = cur;
		}
	}

	// 注視点（青・参考表示）
	{
		ImVec2 s = worldToScreen(comp(m_lookat, axisH), comp(m_lookat, axisV));
		dl->AddCircleFilled(s, 5.0f, IM_COL32(60, 120, 255, 255));
	}

	// 制御点（緑）＋ホバー判定
	ImVec2 mouse = ImGui::GetIO().MousePos;
	int hoverIdx = -1;
	for (int i = 0; i < (int)m_points.size(); ++i)
	{
		ImVec2 s = worldToScreen(comp(m_points[i], axisH), comp(m_points[i], axisV));
		float dx = mouse.x - s.x, dy = mouse.y - s.y;
		bool isNear = (dx * dx + dy * dy) <= 8.0f * 8.0f;
		if (hovered && isNear) hoverIdx = i;

		ImU32 col = (i == m_canvasDrag || (hovered && isNear))
			? IM_COL32(120, 255, 120, 255) : IM_COL32(40, 200, 40, 255);
		dl->AddCircleFilled(s, 6.0f, col);

		char num[8];
		snprintf(num, sizeof(num), "%d", i);
		dl->AddText(ImVec2(s.x + 7, s.y - 7), IM_COL32(200, 255, 200, 255), num);
	}

	// --- インタラクション ---
	// クリックした瞬間：近傍点があればドラッグ開始、無ければ（許可時）追加
	if (hovered && ImGui::IsMouseClicked(0))
	{
		if (hoverIdx >= 0)
		{
			m_canvasDrag = hoverIdx;
			m_canvasDragView = axisV;	// この面がドラッグ主
		}
		else if (allowAddRemove)
		{
			Vector3 np(0, 150, 0);	// 他軸は既定
			*compPtr(np, axisH) = screenToWorldH(mouse.x);
			*compPtr(np, axisV) = screenToWorldV(mouse.y);
			m_points.push_back(np);
			m_speeds.push_back(1.0f);
		}
	}

	// ドラッグ中：この面が主のときだけ、2軸成分を更新
	if (m_canvasDrag >= 0 && m_canvasDragView == axisV
		&& m_canvasDrag < (int)m_points.size() && ImGui::IsMouseDown(0))
	{
		*compPtr(m_points[m_canvasDrag], axisH) = screenToWorldH(mouse.x);
		*compPtr(m_points[m_canvasDrag], axisV) = screenToWorldV(mouse.y);
	}
	if (ImGui::IsMouseReleased(0)) { m_canvasDrag = -1; m_canvasDragView = -1; }

	// 右クリックで削除（4点は維持）
	if (allowAddRemove && hovered && ImGui::IsMouseClicked(1) && hoverIdx >= 0 && m_points.size() > 4)
	{
		m_points.erase(m_points.begin() + hoverIdx);
		if (hoverIdx < (int)m_speeds.size()) m_speeds.erase(m_speeds.begin() + hoverIdx);
	}
}

void SplineCamera::DebugUI()
{
	ImGui::Begin("Spline Camera");

	ImGui::Checkbox("Active (C key)", &m_active);
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &m_loop);
	ImGui::SameLine();
	ImGui::Checkbox("Show Path", &m_showPath);

	ImGui::Text("t = %.2f   len = %.0f   dist = %.0f", m_t, m_totalLen, m_dist);

	ImGui::DragFloat3("Lookat", &m_lookat.x, 1.0f);

	ImGui::End();	// Spline Camera ウィンドウ終了

	// ===== 曲線パラメータ（別ウィンドウ） =====
	ImGui::Begin("Curve Editor");

	// --- 2Dキャンバスで曲線を編集 ---
	ImGui::SliderFloat("View", &m_editViewHalf, 200.0f, 2000.0f, "range %.0f");

	ImGui::Text("Top view (X-Z):  left drag=move / left click=add / right click=remove");
	DrawEditCanvas("##topXZ", 0, 2, true);	// 俯瞰：X横・Z縦、追加削除あり

	ImGui::Text("Side view (X-Y):  left drag=height");
	DrawEditCanvas("##sideXY", 0, 1, false);	// 側面：X横・Y縦、高さ編集

	ImGui::Separator();
	ImGui::Text("Control Points (%d)", (int)m_points.size());

	// 速度配列のサイズを制御点数に常に同期（保険）
	m_speeds.resize(m_points.size(), 1.0f);

	// 位置だけ編集（速度は「Spline Speed」ウィンドウに集約）
	for (int i = 0; i < (int)m_points.size(); ++i)
	{
		std::string label = "P" + std::to_string(i);
		ImGui::DragFloat3(label.c_str(), &m_points[i].x, 1.0f);
	}

	if (ImGui::Button("Add")) {
		Vector3 p = m_points.empty() ? Vector3(0, 150, 0) : m_points.back() + Vector3(50, 0, 50);
		m_points.push_back(p);
		m_speeds.push_back(m_speeds.empty() ? 1.0f : m_speeds.back());
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove")) {
		if (m_points.size() > 4) {
			m_points.pop_back();
			m_speeds.pop_back();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		ResetDefault();
	}

	ImGui::End();	// Curve Editor ウィンドウ終了

	// ===== 速度パラメータ（別ウィンドウ） =====
	ImGui::Begin("Spline Speed");

	// マスター速度（経路全体の基準速度：ワールド単位/秒）
	ImGui::SliderFloat("Master Speed", &m_speed, 10.0f, 1500.0f);

	ImGui::Separator();
	ImGui::Text("Per-point speed multiplier");

	// 速度配列を制御点数に同期（保険）
	m_speeds.resize(m_points.size(), 1.0f);

	// 制御点ごとの速度倍率（区間は隣接点間を SpeedAt() が線形補間）
	for (int i = 0; i < (int)m_speeds.size(); ++i)
	{
		std::string label = "P" + std::to_string(i) + " speed";
		ImGui::DragFloat(label.c_str(), &m_speeds[i], 0.05f, 0.05f, 5.0f, "%.2f x");
	}

	// --- 速度グラフ（横=経路パラメータ / 縦=実効速度倍率） ---
	ImGui::Separator();
	ImGui::Text("Effective speed along path");

	if (m_points.size() >= 4)
	{
		int   n    = (int)m_points.size();
		float uMax = m_loop ? (float)n : (float)(n - 1);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 g0 = ImGui::GetCursorScreenPos();
		float  gw = ImGui::GetContentRegionAvail().x;
		if (gw < 50.0f) gw = 50.0f;
		ImVec2 gsize(gw, 120.0f);
		ImGui::InvisibleButton("##speedGraph", gsize);
		ImVec2 g1 = ImVec2(g0.x + gsize.x, g0.y + gsize.y);

		// 背景・枠
		dl->AddRectFilled(g0, g1, IM_COL32(25, 25, 30, 255));
		dl->AddRect(g0, g1, IM_COL32(90, 90, 100, 255));

		// 縦軸スケール：倍率の最大値（最低でも 1.0 は確保、少し余白）
		float vmax = 1.0f;
		for (float s : m_speeds) vmax = (s > vmax) ? s : vmax;
		vmax *= 1.15f;

		// 座標変換：u(0..uMax) → x、倍率(0..vmax) → y（下向き反転）
		auto toX = [&](float u)   { return g0.x + (u / uMax) * gsize.x; };
		auto toY = [&](float spd) { return g1.y - (spd / vmax) * gsize.y; };

		// 倍率 1.0 の基準線（グレー）
		{
			float y1 = toY(1.0f);
			dl->AddLine(ImVec2(g0.x, y1), ImVec2(g1.x, y1), IM_COL32(70, 70, 80, 255));
			dl->AddText(ImVec2(g0.x + 2, y1 - 14), IM_COL32(120, 120, 130, 255), "1.0x");
		}

		// 制御点の位置（u=整数）に縦ガイド線＋番号
		int guideCount = m_loop ? n : (n - 1);
		for (int i = 0; i <= guideCount; ++i)
		{
			float gx = toX((float)i);
			dl->AddLine(ImVec2(gx, g0.y), ImVec2(gx, g1.y), IM_COL32(55, 80, 55, 255));
			char num[8];
			snprintf(num, sizeof(num), "%d", i % n);
			dl->AddText(ImVec2(gx + 2, g1.y - 14), IM_COL32(120, 200, 120, 255), num);
		}

		// 実効速度倍率の折れ線（黄）
		int samples = 128;
		ImVec2 prev = ImVec2(toX(0.0f), toY(SpeedAt(0.0f)));
		for (int i = 1; i <= samples; ++i)
		{
			float u = uMax * (float)i / (float)samples;
			ImVec2 cur = ImVec2(toX(u), toY(SpeedAt(u)));
			dl->AddLine(prev, cur, IM_COL32(230, 220, 40, 255), 2.0f);
			prev = cur;
		}

		// 現在の再生位置（赤い縦線）
		if (m_active)
		{
			float px = toX(m_t);
			dl->AddLine(ImVec2(px, g0.y), ImVec2(px, g1.y), IM_COL32(255, 60, 60, 255), 2.0f);
		}
	}
	else
	{
		ImGui::TextDisabled("(need >= 4 control points)");
	}

	ImGui::End();	// Spline Speed ウィンドウ終了
}
