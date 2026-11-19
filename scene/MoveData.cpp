#include "MoveData.h"
#include "../Json.h"
#include "../system/utility.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	namespace fs = std::filesystem;

	// 技名は日本語も入るので、UTF-8 → ワイド文字に変換してからパスにする（CameraLibrary と同じ）
	fs::path ToPath(const std::string& utf8) { return fs::path(utility::utf8_to_wide_winapi(utf8)); }
	fs::path RootPath()                      { return ToPath("MoveLibrary"); }
	fs::path FilePath(const std::string& name) { return RootPath() / ToPath(name + ".json"); }

	// 1キーで何フレーム（60fps）進むか。速度の倍率もここで掛ける
	float KeysPerFrame(float keysPerSec, float speed)
	{
		return std::max(keysPerSec * speed, 0.01f) / 60.0f;
	}
}

// ---------------------------------------------------------------- MoveData

int MoveData::TotalFrames(float keysPerSec) const
{
	// 先頭のキーが 1F、最後のキーが最終フレームになるように数える
	const int span = std::max(clipEnd - clipBegin, 0);
	return (int)std::lround(span / KeysPerFrame(keysPerSec, speed)) + 1;
}

int MoveData::KeyAtFrame(int frame, float keysPerSec) const
{
	const float k = (frame - 1) * KeysPerFrame(keysPerSec, speed);
	return std::clamp(clipBegin + (int)k, clipBegin, std::max(clipEnd, clipBegin));
}

bool MoveData::FrameData(float keysPerSec, int& startup, int& active, int& recovery) const
{
	if (hitboxes.empty()) return false;

	const int total = TotalFrames(keysPerSec);
	int first = total, last = 1;
	for (const HitboxKey& h : hitboxes)
	{
		first = std::min(first, h.begin);
		last  = std::max(last,  h.end);
	}
	first = std::clamp(first, 1, total);
	last  = std::clamp(last, first, total);

	startup  = first;
	active   = last - first + 1;	// 判定どうしの隙間も持続に含める（多段技の数え方）
	recovery = total - last;
	return true;
}

bool MoveData::IsActiveAt(int frame) const
{
	for (const HitboxKey& h : hitboxes)
		if (frame >= h.begin && frame <= h.end) return true;
	return false;
}

// ---------------------------------------------------------------- 姿勢キー

BoneTrack* MoveData::FindTrack(const std::string& bone)
{
	for (BoneTrack& t : poseLayer) if (t.bone == bone) return &t;
	return nullptr;
}

const BoneTrack* MoveData::FindTrack(const std::string& bone) const
{
	for (const BoneTrack& t : poseLayer) if (t.bone == bone) return &t;
	return nullptr;
}

const PoseKey* MoveData::FindPoseKey(const std::string& bone, int frame) const
{
	const BoneTrack* t = FindTrack(bone);
	if (!t) return nullptr;
	for (const PoseKey& k : t->keys) if (k.frame == frame) return &k;
	return nullptr;
}

void MoveData::SetPoseKey(const std::string& bone, int frame, const float q[4])
{
	BoneTrack* t = FindTrack(bone);
	if (!t)
	{
		poseLayer.push_back({ bone, {} });
		t = &poseLayer.back();
	}

	// frame の小さい順を保ったまま入れる（同じフレームなら上書き）
	auto it = std::lower_bound(t->keys.begin(), t->keys.end(), frame,
		[](const PoseKey& k, int f) { return k.frame < f; });
	if (it == t->keys.end() || it->frame != frame) it = t->keys.insert(it, PoseKey{ frame });
	std::copy(q, q + 4, it->q);
}

bool MoveData::RemovePoseKey(const std::string& bone, int frame)
{
	BoneTrack* t = FindTrack(bone);
	if (!t) return false;

	auto it = std::find_if(t->keys.begin(), t->keys.end(), [&](const PoseKey& k) { return k.frame == frame; });
	if (it == t->keys.end()) return false;
	t->keys.erase(it);
	if (t->keys.empty()) ClearTrack(bone);
	return true;
}

void MoveData::ClearTrack(const std::string& bone)
{
	poseLayer.erase(std::remove_if(poseLayer.begin(), poseLayer.end(),
		[&](const BoneTrack& t) { return t.bone == bone; }), poseLayer.end());
}

// ---------------------------------------------------------------- JSON

JsonValue MoveData::ToJson() const
{
	JsonValue root = JsonValue::MakeObject();
	root.Set("version",   JsonValue(1));
	root.Set("name",      JsonValue(name));
	root.Set("motion",    JsonValue(motion));
	root.Set("clipBegin", JsonValue(clipBegin));
	root.Set("clipEnd",   JsonValue(clipEnd));
	root.Set("speed",     JsonValue(speed));

	JsonValue arr = JsonValue::MakeArray();
	for (const HitboxKey& h : hitboxes)
	{
		JsonValue o = JsonValue::MakeObject();
		o.Set("begin", JsonValue(h.begin));
		o.Set("end",   JsonValue(h.end));
		o.Set("bone",  JsonValue(h.bone));
		o.Set("offX",  JsonValue(h.offX));
		o.Set("offY",  JsonValue(h.offY));
		o.Set("w",     JsonValue(h.w));
		o.Set("h",     JsonValue(h.h));
		arr.Push(o);
	}
	root.Set("hitboxes", arr);

	// 姿勢の編集: [{ "bone": "...", "keys": [{ "f": 10, "q": [x, y, z, w] }, ...] }, ...]
	JsonValue pose = JsonValue::MakeArray();
	for (const BoneTrack& t : poseLayer)
	{
		JsonValue keys = JsonValue::MakeArray();
		for (const PoseKey& k : t.keys)
		{
			JsonValue q = JsonValue::MakeArray();
			for (float c : k.q) q.Push(JsonValue(c));

			JsonValue o = JsonValue::MakeObject();
			o.Set("f", JsonValue(k.frame));
			o.Set("q", q);
			keys.Push(o);
		}
		JsonValue tr = JsonValue::MakeObject();
		tr.Set("bone", JsonValue(t.bone));
		tr.Set("keys", keys);
		pose.Push(tr);
	}
	root.Set("pose", pose);
	return root;
}

bool MoveData::FromJson(const JsonValue& v)
{
	if (v.GetType() != JsonValue::Type::Object) return false;

	name      = v["name"].AsString(name);
	motion    = v["motion"].AsString(motion);
	clipBegin = (int)v["clipBegin"].AsNumber(clipBegin);
	clipEnd   = (int)v["clipEnd"].AsNumber(clipEnd);
	speed     = v["speed"].AsFloat(speed);

	hitboxes.clear();
	const JsonValue& arr = v["hitboxes"];
	for (size_t i = 0; i < arr.Size(); ++i)
	{
		const JsonValue& o = arr.At(i);
		HitboxKey h;
		h.begin = (int)o["begin"].AsNumber(h.begin);
		h.end   = (int)o["end"].AsNumber(h.end);
		h.bone  = o["bone"].AsString(h.bone);
		h.offX  = o["offX"].AsFloat(h.offX);
		h.offY  = o["offY"].AsFloat(h.offY);
		h.w     = o["w"].AsFloat(h.w);
		h.h     = o["h"].AsFloat(h.h);
		hitboxes.push_back(h);
	}

	// 姿勢の編集（無いファイルもある＝古い保存形式）
	poseLayer.clear();
	const JsonValue& pose = v["pose"];
	for (size_t i = 0; i < pose.Size(); ++i)
	{
		const JsonValue& tr = pose.At(i);
		const std::string bone = tr["bone"].AsString("");
		if (bone.empty()) continue;

		const JsonValue& keys = tr["keys"];
		for (size_t j = 0; j < keys.Size(); ++j)
		{
			const JsonValue& k = keys.At(j);
			const JsonValue& q = k["q"];
			if (q.Size() != 4) continue;

			float qv[4];
			for (size_t c = 0; c < 4; ++c) qv[c] = q.At(c).AsFloat(c == 3 ? 1.0f : 0.0f);
			SetPoseKey(bone, (int)k["f"].AsNumber(1), qv);	// 並べ替えもここでされる
		}
	}
	return !motion.empty();
}

// ---------------------------------------------------------------- MoveLibrary

std::vector<std::string> MoveLibrary::List()
{
	std::vector<std::string> out;
	try
	{
		if (!fs::exists(RootPath())) return out;
		for (const auto& e : fs::directory_iterator(RootPath()))
		{
			if (!e.is_regular_file() || e.path().extension() != L".json") continue;
			out.push_back(utility::PathToUtf8String(e.path().stem()));
		}
	}
	catch (const std::exception&) {}	// フォルダが途中で消えた等。空のまま返す

	std::sort(out.begin(), out.end());
	return out;
}

bool MoveLibrary::Save(const MoveData& move, std::string* error)
{
	if (move.name.empty())   { if (error) *error = "技名が空です";             return false; }
	if (move.motion.empty()) { if (error) *error = "素材のモーションがありません"; return false; }

	try
	{
		fs::create_directories(RootPath());
		std::ofstream ofs(FilePath(move.name), std::ios::binary);
		if (!ofs) { if (error) *error = "ファイルを開けません"; return false; }

		const std::string text = move.ToJson().Dump(2);
		ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
		if (!ofs) { if (error) *error = "書き込みに失敗しました"; return false; }
	}
	catch (const std::exception& e)
	{
		if (error) *error = e.what();
		return false;
	}
	return true;
}

bool MoveLibrary::Load(const std::string& name, MoveData& out, std::string* error)
{
	try
	{
		std::ifstream ifs(FilePath(name), std::ios::binary);
		if (!ifs) { if (error) *error = "ファイルが見つかりません"; return false; }

		const std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		JsonValue root;
		std::string perr;
		if (!JsonValue::Parse(text, root, &perr)) { if (error) *error = "JSONとして読めません: " + perr; return false; }

		MoveData loaded;
		if (!loaded.FromJson(root)) { if (error) *error = "技データとして読めません"; return false; }

		loaded.name = name;		// ファイル名を正とする（改名しても食い違わない）
		out = std::move(loaded);
	}
	catch (const std::exception& e)
	{
		if (error) *error = e.what();
		return false;
	}
	return true;
}

bool MoveLibrary::Remove(const std::string& name)
{
	try
	{
		std::error_code ec;
		return fs::remove(FilePath(name), ec);
	}
	catch (const std::exception&)
	{
		return false;
	}
}
