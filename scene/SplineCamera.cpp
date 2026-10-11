#include "SplineCamera.h"
#include "../system/imgui/imgui.h"
#include "../system/LineDrawer.h"
#include "../system/renderer.h"
#include "../CameraTake.h"
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
	m_rolls.assign(m_points.size(), 0.0f);	// roll は全点0（水平）
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

// u（連続パラメータ）位置のカメラ座標を求める（補間方法は m_interp で切り替え）
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

	const Vector3& p1 = m_points[idx(seg)];
	const Vector3& p2 = m_points[idx(seg + 1)];

	// 補間なし：区間の間は始点に留まり、次の制御点へ瞬間移動する
	if (m_interp == Interp::None) return p1;

	// 直線：制御点どうしを直線で結ぶ
	if (m_interp == Interp::Linear) return Vector3::Lerp(p1, p2, t);

	// 曲線（Catmull-Rom）：前後1点ずつを加えた4点で評価
	const Vector3& p0 = m_points[idx(seg - 1)];
	const Vector3& p3 = m_points[idx(seg + 2)];

	return Vector3::CatmullRom(p0, p1, p2, p3, t);
}

// u位置の実効速度倍率を求める（補間方法は m_interp に従う）
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

	float s1 = m_speeds[idx(seg)];
	float s2 = m_speeds[idx(seg + 1)];

	// 補間なし：区間ごとに一定
	if (m_interp == Interp::None)   return std::max(s1, 0.05f);

	// 直線：隣の値へ線形に変化（制御点で加速度が折れる）
	if (m_interp == Interp::Linear) return std::max(s1 + (s2 - s1) * t, 0.05f);

	// 位置カーブ(Eval)と同じ4点ステンシル。スカラー速度を x に詰めてCatmull-Rom補間する。
	float s0 = m_speeds[idx(seg - 1)];
	float s3 = m_speeds[idx(seg + 2)];

	Vector3 r = Vector3::CatmullRom(
		Vector3(s0, 0, 0), Vector3(s1, 0, 0), Vector3(s2, 0, 0), Vector3(s3, 0, 0), t);

	// Catmull-Romは近傍値をオーバーシュートし得る→0以下だと停止・逆走するのでUI下限に合わせてクランプ
	return std::max(r.x, 0.05f);
}

// u位置の roll[度] を求める（補間方法は m_interp に従う。roll は負もあり得るのでクランプ無し）
float SplineCamera::RollAt(float u) const
{
	int n = (int)m_points.size();
	if (n < 4 || m_rolls.size() != m_points.size()) return 0.0f;

	int   seg = (int)floorf(u);
	float t   = u - seg;

	auto idx = [&](int k) -> int {
		if (m_loop) return ((k % n) + n) % n;
		return std::clamp(k, 0, n - 1);
	};

	float r1 = m_rolls[idx(seg)];
	float r2 = m_rolls[idx(seg + 1)];

	if (m_interp == Interp::None)   return r1;					// 区間ごとに一定
	if (m_interp == Interp::Linear) return r1 + (r2 - r1) * t;	// 線形

	float r0 = m_rolls[idx(seg - 1)];
	float r3 = m_rolls[idx(seg + 2)];

	Vector3 r = Vector3::CatmullRom(
		Vector3(r0, 0, 0), Vector3(r1, 0, 0), Vector3(r2, 0, 0), Vector3(r3, 0, 0), t);

	return r.x;	// 度
}

// 可視化専用の評価。補間なしのときは経路が点だけになって見えないので、直線でつないで返す。
// 再生位置そのものには使わない（そちらは Eval を使う）。
Vector3 SplineCamera::EvalForDraw(float u) const
{
	if (m_interp != Interp::None) return Eval(u);

	int n = (int)m_points.size();
	if (n < 4) return m_lookat;

	int   seg = (int)floorf(u);
	float t   = u - seg;

	auto idx = [&](int k) -> int {
		if (m_loop) return ((k % n) + n) % n;
		return std::clamp(k, 0, n - 1);
	};

	return Vector3::Lerp(m_points[idx(seg)], m_points[idx(seg + 1)], t);
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

// 曲線パラメータ u から距離（弧長）へ変換する（ArcLengthToU の逆）
// 弧長パラメータ化を切ったときに、表示と終端判定用の距離を求めるのに使う。
float SplineCamera::ArcLengthAtU(float u) const
{
	int M = (int)m_arc.size() - 1;
	if (M < 1) return 0.0f;

	float uMax = (float)M / (float)m_samplesPerSeg;	// BuildArcTable と一致
	if (uMax <= 1e-6f) return 0.0f;

	// u をテーブルの添字に直し、サンプル間は線形補間する
	float sampleIndex = std::clamp(u / uMax * (float)M, 0.0f, (float)M);
	int   j    = std::min((int)sampleIndex, M - 1);
	float frac = sampleIndex - (float)j;

	return m_arc[j] + (m_arc[j + 1] - m_arc[j]) * frac;
}

void SplineCamera::Update(float dt, Camera& cam)
{
	if (!m_active || m_points.size() < 4)
	{
		m_measValid = false;	// 止まっている間の差分は測らない
		return;
	}

	// 弧長テーブルを毎フレーム再構築（ギズモで制御点を編集中でも追従させるため）
	BuildArcTable();

	if (m_totalLen < 1e-4f)
	{
		// 退化ガード（全制御点がほぼ同一など）
		cam.SetPosition(Eval(m_t));
		cam.SetLookat(m_lookat);
		m_measValid = false;
		return;
	}

	m_rolls.resize(m_points.size(), 0.0f);	// サイズ同期（保険）

	// このフレームで出したい速度。ON / OFF どちらも同じ式で比べる
	const float targetSpeed = m_speed * SpeedAt(m_t);

	if (m_useArcLength)
	{
		// --- 弧長パラメータ化 ON：距離で進めて u に翻訳する（等速） ---
		// マスター速度[ワールド単位/秒] × 現在位置の速度倍率（前フレームの u で評価）
		m_dist += targetSpeed * dt;	// 進行を「距離」で加算

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
	}
	else
	{
		// --- 弧長パラメータ化 OFF：u を直接進める（区間ごとに速度がばらつく） ---
		// 比較しやすいよう、1周にかかる時間は ON のときと揃える
		const float uMax   = (float)(m_loop ? m_points.size() : m_points.size() - 1);
		const float lapSec = m_totalLen / std::max(m_speed, 1e-3f);	// ON のときの1周の秒数
		m_t += (uMax / std::max(lapSec, 1e-3f)) * SpeedAt(m_t) * dt;

		if (m_loop)
		{
			while (m_t >= uMax) m_t -= uMax;	// u でループ
		}
		else if (m_t > uMax)
		{
			m_t = uMax;		// 端で停止
			m_active = false;
		}

		m_dist = ArcLengthAtU(m_t);	// 表示と終端判定のために距離も求めておく
	}

	Vector3 pos = Eval(m_t);

	// --- 等速の確認：実際にカメラが動いた距離 ÷ 経過時間 ---
	// 1フレームの移動は数単位なので、直線距離と曲線に沿った距離の差は無視できる
	if (m_measValid && dt > 1e-6f)
	{
		m_measSpeed  = (pos - m_measPrevPos).Length() / dt;
		m_measTarget = targetSpeed;

		const float ratio = (targetSpeed > 1e-3f) ? m_measSpeed / targetSpeed : 0.0f;
		m_speedHist[m_speedHistPos] = ratio;
		m_speedHistPos   = (m_speedHistPos + 1) % kSpeedHistory;
		m_speedHistCount = std::min(m_speedHistCount + 1, kSpeedHistory);
	}
	m_measPrevPos = pos;
	m_measValid   = true;

	// 前方軸まわりに world-up を roll だけ回してダッチアングルを作る
	Vector3 fwd = m_lookat - pos;
	Vector3 up(0, 1, 0);
	if (fwd.LengthSquared() > 1e-8f) {
		fwd.Normalize();
		float rollRad = RollAt(m_t) * 0.01745329252f;	// 度→ラジアン
		Matrix4x4 rollM = Matrix4x4::CreateFromAxisAngle(fwd, rollRad);
		up = Vector3::Transform(Vector3(0, 1, 0), rollM);
	}

	cam.SetPosition(pos);		// 位置は経路上
	cam.SetLookat(m_lookat);	// 常に固定の注視点を見る
	cam.SetUP(up);				// 傾いた up でダッチアングル
}

void SplineCamera::DrawVisualization()
{
	if (!m_showPath || m_points.size() < 4) return;

	int   n    = (int)m_points.size();
	float uMax = m_loop ? (float)n : (float)(n - 1);
	int   samples = (int)(uMax * 24);

	// 経路（黄色線）
	Vector3 prev = EvalForDraw(0);
	SetLineWidth(2);
	for (int i = 1; i <= samples; ++i) {
		Vector3 cur = EvalForDraw(uMax * i / samples);
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
		Vector3 prev = EvalForDraw(0);
		for (int i = 1; i <= samples; ++i)
		{
			Vector3 cur = EvalForDraw(uMax * i / samples);
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

void SplineCamera::DrawPlaybackUI()
{
	// ループOFFで終端に居るときに有効へ戻したら、先頭から流し直す
	if (ImGui::Checkbox("有効（Cキー）##Active (C key)", &m_active))
	{
		if (m_active && !m_loop && m_dist >= m_totalLen - 1e-4f) Play();
	}
	ImGui::SameLine();
	if (ImGui::Button("先頭から##Play from start")) Play();
	ImGui::SameLine();
	ImGui::Checkbox("ループ##Loop", &m_loop);
	ImGui::SameLine();
	ImGui::Checkbox("経路を表示##Show Path", &m_showPath);

	// 補間方法。曲線に戻したいときは「曲線（Catmull-Rom）」を選ぶ
	const char* interpNames[] = { "曲線（Catmull-Rom）", "直線", "なし（点で飛ぶ）" };
	int interp = (int)m_interp;
	if (ImGui::Combo("補間##Interp", &interp, interpNames, IM_ARRAYSIZE(interpNames)))
	{
		m_interp = (Interp)interp;
		BuildArcTable();	// 経路の長さが変わるので作り直す
		ResetSpeedMeasure();
	}

	// 弧長パラメータ化の ON / OFF。OFF は u を直接進める（比較用）
	if (ImGui::Checkbox("弧長パラメータ化（等速）##Arc length", &m_useArcLength))
	{
		// 切り替えた瞬間に位置が飛ばないよう、進行量を合わせる
		if (m_useArcLength) m_dist = ArcLengthAtU(m_t);
		ResetSpeedMeasure();	// 前の設定の記録が混ざらないように
	}

	ImGui::Text("パラメータ = %.2f   全長 = %.0f   移動距離 = %.0f", m_t, m_totalLen, m_dist);
	if (!m_useArcLength)
		ImGui::TextDisabled("弧長パラメータ化 OFF：区間ごとに速度がばらつきます");

	DrawSpeedMeasureUI();

	ImGui::DragFloat3("注視点##Lookat", &m_lookat.x, 1.0f);

}

void SplineCamera::DrawCurveEditorUI()
{
	// --- 2Dキャンバスで曲線を編集 ---
	ImGui::SliderFloat("表示範囲##View", &m_editViewHalf, 200.0f, 2000.0f, "範囲 %.0f");

	ImGui::Text("上面図（X-Z）: 左ドラッグ=移動／左クリック=追加／右クリック=削除");
	DrawEditCanvas("##topXZ", 0, 2, true);	// 俯瞰：X横・Z縦、追加削除あり

	ImGui::Text("側面図（X-Y）: 左ドラッグ=高さ変更");
	DrawEditCanvas("##sideXY", 0, 1, false);	// 側面：X横・Y縦、高さ編集

	ImGui::Separator();
	ImGui::Text("制御点（%d個）", (int)m_points.size());

	// 速度配列のサイズを制御点数に常に同期（保険）
	m_speeds.resize(m_points.size(), 1.0f);

	// 位置だけ編集（速度は「Spline Speed」ウィンドウに集約）
	for (int i = 0; i < (int)m_points.size(); ++i)
	{
		std::string label = "P" + std::to_string(i);
		ImGui::DragFloat3(label.c_str(), &m_points[i].x, 1.0f);
	}

	if (ImGui::Button("追加##Add")) {
		Vector3 p = m_points.empty() ? Vector3(0, 150, 0) : m_points.back() + Vector3(50, 0, 50);
		m_points.push_back(p);
		m_speeds.push_back(m_speeds.empty() ? 1.0f : m_speeds.back());
	}
	ImGui::SameLine();
	if (ImGui::Button("削除##Remove")) {
		if (m_points.size() > 4) {
			m_points.pop_back();
			m_speeds.pop_back();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("初期化##Reset")) {
		ResetDefault();
	}

}

void SplineCamera::DrawSpeedUI()
{
	// マスター速度（経路全体の基準速度：ワールド単位/秒）
	ImGui::SliderFloat("基準速度##Master Speed", &m_speed, 10.0f, 1500.0f);

	ImGui::Separator();
	ImGui::Text("制御点ごとの速度倍率");

	// 速度配列を制御点数に同期（保険）
	m_speeds.resize(m_points.size(), 1.0f);

	// 制御点ごとの速度倍率（区間は隣接点間を SpeedAt() が線形補間）
	for (int i = 0; i < (int)m_speeds.size(); ++i)
	{
		std::string label = "P" + std::to_string(i) + " 速度##P" + std::to_string(i) + " speed";
		ImGui::DragFloat(label.c_str(), &m_speeds[i], 0.05f, 0.05f, 5.0f, "%.2f x");
	}

	// --- 速度グラフ（横=経路パラメータ / 縦=実効速度倍率） ---
	ImGui::Separator();
	ImGui::Text("経路上の実効速度（ノブをドラッグして調整）");

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

		// 縦軸スケール：固定レンジ（DragFloatの上限と一致→ノブ位置と1:1対応でドラッグが安定）
		const float vmax = 5.0f;

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

		// --- 各制御点のノブを直接ドラッグして倍率を調整 ---
		ImVec2 mouse = ImGui::GetIO().MousePos;
		bool   hovered = ImGui::IsItemHovered();	// InvisibleButton("##speedGraph") 領域
		float  colW = gsize.x / uMax;				// 制御点1つ分の横幅

		// クリックで最寄りの制御点列を掴む（列幅の45%以内＝縦ストリップをフェーダー的に掴める）
		if (hovered && ImGui::IsMouseClicked(0))
		{
			int   best = -1;
			float bestdx = colW * 0.45f;
			for (int i = 0; i < n; ++i)
			{
				float dx = fabsf(mouse.x - toX((float)i));
				if (dx < bestdx) { bestdx = dx; best = i; }
			}
			m_speedDrag = best;
		}

		// ドラッグ中：マウスYを倍率へ変換して反映
		if (m_speedDrag >= 0 && m_speedDrag < (int)m_speeds.size() && ImGui::IsMouseDown(0))
		{
			float spd = (g1.y - mouse.y) / gsize.y * vmax;	// screen→倍率（下向き反転の逆）
			m_speeds[m_speedDrag] = std::clamp(spd, 0.05f, 5.0f);
		}
		if (ImGui::IsMouseReleased(0)) m_speedDrag = -1;

		// ノブ本体（制御点ごとに円。u=整数で SpeedAt は m_speeds[i] を通るので曲線上に乗る）
		for (int i = 0; i < n; ++i)
		{
			ImVec2 c(toX((float)i), toY(std::clamp(m_speeds[i], 0.0f, vmax)));
			bool  activeKnob = (i == m_speedDrag);
			ImU32 col = activeKnob ? IM_COL32(255, 230, 120, 255) : IM_COL32(230, 180, 40, 255);
			float rad = activeKnob ? 7.0f : 5.5f;
			dl->AddCircleFilled(c, rad, col);
			dl->AddCircle(c, rad, IM_COL32(30, 30, 30, 255));
		}

		// ドラッグ中は数値ラベルを表示
		if (m_speedDrag >= 0 && m_speedDrag < (int)m_speeds.size())
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "%.2f x", m_speeds[m_speedDrag]);
			dl->AddText(ImVec2(toX((float)m_speedDrag) + 8, toY(m_speeds[m_speedDrag]) - 16),
				IM_COL32(255, 255, 255, 255), buf);
		}
	}
	else
	{
		ImGui::TextDisabled("（制御点が4個以上必要です）");
	}

}

void SplineCamera::DrawRollUI()
{
	ImGui::TextDisabled("制御点ごとのダッチアングル（度）");

	m_rolls.resize(m_points.size(), 0.0f);	// サイズ同期（保険）

	for (int i = 0; i < (int)m_rolls.size(); ++i)
	{
		std::string label = "P" + std::to_string(i) + " ロール##P" + std::to_string(i) + " roll";
		ImGui::SliderFloat(label.c_str(), &m_rolls[i], -90.0f, 90.0f, "%.0f 度");
	}

	if (ImGui::Button("ロールを初期化##Reset roll")) std::fill(m_rolls.begin(), m_rolls.end(), 0.0f);
}


void  SplineCamera::CaptureTo(CameraTake& take, const Vector3& origin) const
{
	take.origin    = origin;
	take.lookat	   = m_lookat-origin;	//絶対→相対
	take.baseSpeed = m_speed;
	take.loop	   = m_loop;

	take.points.clear();
	take.points.reserve(m_points.size());

	for (size_t i = 0; i < m_points.size(); ++i)
	{
		CamPoint p;
		p.pos = m_points[i]-origin;

		p.speed = (i < m_speeds.size()) ? m_speeds[i] : 1.0f;
		p.roll	= (i < m_rolls.size()) ? m_rolls[i] : 0.0f;

		take.points.push_back(p);
	}


}


bool SplineCamera::RestoreFrom(const CameraTake& take, const Vector3& origin)
{
	if (take.points.size() < 4) return false;

	m_points.clear();
	m_speeds.clear();
	m_rolls.clear();

	m_points.reserve(take.points.size());
	m_speeds.reserve(take.points.size());
	m_rolls	.reserve(take.points.size());


	for (const CamPoint& p : take.points)
	{
		m_points.push_back(p.pos + origin);
		m_speeds.push_back(p.speed);
		m_rolls	.push_back(p.roll);
	}

	m_lookat = take.lookat + origin;
	m_speed = take.baseSpeed;
	m_loop = take.loop;

	m_t	   = 0.0f;
	m_dist = 0.0f;

	m_canvasDrag	 = -1;
	m_canvasDragView = -1;
	m_speedDrag		 = -1;

	BuildArcTable();
	ResetSpeedMeasure();	// 別の経路に変わるので、前の記録は捨てる
	return true;

}


// 先頭から再生し直す。
// ループOFFで終端に着くと m_dist が m_totalLen のまま残るので、
// 巻き戻さずに m_active を立て直しても次のフレームで即停止してしまう。
void SplineCamera::Play()
{
	m_dist = 0.0f;
	m_t = 0.0f;
	m_active = true;
	ResetSpeedMeasure();
}

// 計測をやり直す。巻き戻しや設定の切り替えで、前の記録や位置の飛びが混ざらないようにする
void SplineCamera::ResetSpeedMeasure()
{
	m_speedHistPos   = 0;
	m_speedHistCount = 0;
	m_measValid  = false;
	m_measSpeed  = 0.0f;
	m_measTarget = 0.0f;
}

// 等速になっているかを数値とグラフで見せる
void SplineCamera::DrawSpeedMeasureUI()
{
	if (!ImGui::CollapsingHeader("等速の確認（デバッグ）##Speed check", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (m_speedHistCount == 0)
	{
		ImGui::TextDisabled("再生すると計測が始まります");
		return;
	}

	// 履歴から 最小・最大・平均 を出す（並び順は関係ないので先頭から舐める）
	float mn = m_speedHist[0], mx = m_speedHist[0], sum = 0.0f;
	for (int i = 0; i < m_speedHistCount; ++i)
	{
		const float v = m_speedHist[i];
		mn = std::min(mn, v);
		mx = std::max(mx, v);
		sum += v;
	}
	const float avg    = sum / (float)m_speedHistCount;
	const float spread = (mx - mn) * 100.0f;	// ばらつき[%]。1.00 からの振れ幅の合計

	ImGui::Text("目標速度 %7.1f   実測速度 %7.1f  （単位/秒）", m_measTarget, m_measSpeed);
	ImGui::Text("目標との比  最小 %.3f   最大 %.3f   平均 %.3f", mn, mx, avg);

	// ばらつきが小さければ等速と判定する（弧長テーブルの分割による誤差を見込んで 2%）
	if (spread < 2.0f)
		ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ばらつき %.2f %%  → 等速です", spread);
	else
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "ばらつき %.1f %%  → 速度が変わっています", spread);

	// 比の推移。1.0 の横一直線になれば等速。溜まりきったらリングの古い側から並べる
	const int offset = (m_speedHistCount == kSpeedHistory) ? m_speedHistPos : 0;
	char overlay[32];
	snprintf(overlay, sizeof(overlay), "比 %.3f", m_measTarget > 1e-3f ? m_measSpeed / m_measTarget : 0.0f);
	ImGui::PlotLines("##ratio", m_speedHist, m_speedHistCount, offset,
		overlay, 0.0f, 2.0f, ImVec2(-1.0f, 80.0f));
	ImGui::TextDisabled("グラフの中央の高さが 1.0（目標どおり）。上下 0.0 〜 2.0");

	// 弧長テーブルの細かさ。粗いとサンプル間の直線補間の誤差でわずかに速度が揺れる
	if (ImGui::SliderInt("弧長テーブルの分割数##Samples per seg", &m_samplesPerSeg, 4, 256))
		ResetSpeedMeasure();

	if (ImGui::Button("計測をリセット##Reset measure")) ResetSpeedMeasure();
}