#include "Hurtbox.h"
#include "../Json.h"
#include <algorithm>
#include <fstream>
#include <iterator>

namespace
{
	const char* kFile = "HurtboxConfig.json";

	// 最初の形（以前のコードの表と同じ。余白は縦横同じ）
	std::vector<HurtboxDef> Defaults()
	{
		auto box = [](const char* a, const char* b, float pad) { return HurtboxDef{ a, b, pad, pad, 0.0f, 0.0f }; };
		return {
			box("mixamorig:Hips",         "mixamorig:Spine2",       18.0f),
			box("mixamorig:Neck",         "mixamorig:Head",         11.0f),
			box("mixamorig:RightArm",     "mixamorig:RightForeArm",  8.0f),
			box("mixamorig:RightForeArm", "mixamorig:RightHand",     7.0f),
			box("mixamorig:LeftArm",      "mixamorig:LeftForeArm",   8.0f),
			box("mixamorig:LeftForeArm",  "mixamorig:LeftHand",      7.0f),
			box("mixamorig:RightUpLeg",   "mixamorig:RightLeg",     11.0f),
			box("mixamorig:RightLeg",     "mixamorig:RightFoot",     9.0f),
			box("mixamorig:LeftUpLeg",    "mixamorig:LeftLeg",      11.0f),
			box("mixamorig:LeftLeg",      "mixamorig:LeftFoot",      9.0f),
		};
	}

	std::vector<HurtboxDef> g_defs;
	bool g_loaded = false;
	bool g_dirty  = false;

	bool LoadFile(std::vector<HurtboxDef>& out, std::string* error)
	{
		std::ifstream ifs(kFile, std::ios::binary);
		if (!ifs) { if (error) *error = "ファイルがありません（初期値を使います）"; return false; }

		const std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		JsonValue root;
		std::string perr;
		if (!JsonValue::Parse(text, root, &perr)) { if (error) *error = "JSONとして読めません: " + perr; return false; }

		std::vector<HurtboxDef> defs;
		const JsonValue& arr = root["boxes"];
		for (size_t i = 0; i < arr.Size(); ++i)
		{
			const JsonValue& o = arr.At(i);
			HurtboxDef d;
			d.from = o["from"].AsString("");
			d.to   = o["to"].AsString("");
			d.padX = o["padX"].AsFloat(d.padX);
			d.padY = o["padY"].AsFloat(d.padY);
			d.offX = o["offX"].AsFloat(d.offX);
			d.offY = o["offY"].AsFloat(d.offY);
			if (!d.from.empty() && !d.to.empty()) defs.push_back(d);
		}
		if (defs.empty()) { if (error) *error = "箱が1つもありません"; return false; }
		out = std::move(defs);
		return true;
	}

	void EnsureLoaded()
	{
		if (g_loaded) return;
		g_loaded = true;
		if (!LoadFile(g_defs, nullptr)) g_defs = Defaults();
	}
}

const std::vector<HurtboxDef>& Hurtbox::Defs()
{
	EnsureLoaded();
	return g_defs;
}

std::vector<HurtboxDef>& Hurtbox::Edit()
{
	EnsureLoaded();
	g_dirty = true;
	return g_defs;
}

bool Hurtbox::IsDirty() { return g_dirty; }

bool Hurtbox::Save(std::string* error)
{
	EnsureLoaded();

	JsonValue arr = JsonValue::MakeArray();
	for (const HurtboxDef& d : g_defs)
	{
		JsonValue o = JsonValue::MakeObject();
		o.Set("from", JsonValue(d.from));
		o.Set("to",   JsonValue(d.to));
		o.Set("padX", JsonValue(d.padX));
		o.Set("padY", JsonValue(d.padY));
		o.Set("offX", JsonValue(d.offX));
		o.Set("offY", JsonValue(d.offY));
		arr.Push(o);
	}
	JsonValue root = JsonValue::MakeObject();
	root.Set("version", JsonValue(1));
	root.Set("boxes", arr);

	std::ofstream ofs(kFile, std::ios::binary);
	if (!ofs) { if (error) *error = "ファイルを開けません"; return false; }
	const std::string text = root.Dump(2);
	ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
	if (!ofs) { if (error) *error = "書き込みに失敗しました"; return false; }

	g_dirty = false;
	return true;
}

bool Hurtbox::Reload(std::string* error)
{
	std::vector<HurtboxDef> defs;
	if (!LoadFile(defs, error)) return false;
	g_defs   = std::move(defs);
	g_loaded = true;
	g_dirty  = false;
	return true;
}

void Hurtbox::ResetDefaults()
{
	g_defs   = Defaults();
	g_loaded = true;
	g_dirty  = true;
}

HurtRect Hurtbox::Compute(const HurtboxDef& d, const Vector3& a, const Vector3& b, float scale, float facing)
{
	HurtRect r;
	r.x0 = std::min(a.x, b.x) - d.padX * scale;
	r.x1 = std::max(a.x, b.x) + d.padX * scale;
	r.y0 = std::min(a.y, b.y) - d.padY * scale;
	r.y1 = std::max(a.y, b.y) + d.padY * scale;

	const float ox = d.offX * scale * facing;
	const float oy = d.offY * scale;
	r.x0 += ox; r.x1 += ox;
	r.y0 += oy; r.y1 += oy;

	// 余白をマイナスにしすぎて裏返ったときは、幅 0 の線にしておく
	if (r.x1 < r.x0) r.x0 = r.x1 = r.cx();
	if (r.y1 < r.y0) r.y0 = r.y1 = r.cy();
	return r;
}

float Hurtbox::FacingOf(const Matrix4x4& world)
{
	// モデルの正面はモデル空間の -Z（FBX を左手系に直したあと）。それがワールドでどちらを向くか
	const Vector3 fwd = Vector3::TransformNormal(Vector3(0.0f, 0.0f, -1.0f), world);
	return (fwd.x >= 0.0f) ? 1.0f : -1.0f;
}
