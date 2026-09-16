#include "CameraTake.h"

namespace
{
	// Vector3 を [x, y, z] の配列にする。
	// Json 側が「数値だけの配列は1行に詰める」ので、保存後も読みやすい形になる。
	JsonValue Vec3ToJson(const Vector3& v)
	{
		JsonValue a = JsonValue::MakeArray();
		a.Push(JsonValue(v.x));
		a.Push(JsonValue(v.y));
		a.Push(JsonValue(v.z));
		return a;
	}


	Vector3 Vec3FromJson(const JsonValue& v, const Vector3& def)
	{
		if (v.Size() < 3) return def;

		return Vector3(v.At(0).AsFloat(def.x),
			v.At(1).AsFloat(def.y),
			v.At(2).AsFloat(def.z));


	}


}

JsonValue CameraTake::ToJson() const
{
	JsonValue root = JsonValue::MakeObject();


	root.Set("version",		JsonValue(1));
	root.Set("character",	JsonValue(character));
	root.Set("move",		JsonValue(move));
	root.Set("name",		JsonValue(name));
	root.Set("p1Model",		JsonValue(p1Model));
	root.Set("p2Model",		JsonValue(p2Model));
	
	
	root.Set("origin",		Vec3ToJson(origin));
	root.Set("lookat", 		Vec3ToJson(lookat));
	root.Set("baseSpeed",	JsonValue(baseSpeed));
	root.Set("loop",		JsonValue(loop));
	root.Set("fov",			JsonValue(fov));

	JsonValue arr = JsonValue::MakeArray();
	for (const CamPoint& p : points)
	{
		JsonValue o = JsonValue::MakeObject();
		o.Set("pos",	Vec3ToJson(p.pos));
		o.Set("speed",	JsonValue(p.speed));
		o.Set("roll",	JsonValue(p.roll));
		arr.Push(o);
	}
	root.Set("points", arr);

	return root;


}

bool CameraTake::FromJson(const JsonValue& v)
{
	if (v.GetType() != JsonValue::Type::Object) return false;

	character = v["character"].AsString(character);
	move	  = v["move"].AsString(move);
	name	  = v["name"].AsString(name);
	p1Model	  = v["p1Model"].AsString(p1Model);
	p2Model   = v["p2Model"].AsString(p2Model);


	origin	  = Vec3FromJson(v["origin"], origin);
	lookat	  = Vec3FromJson(v["lookat"], lookat);
	baseSpeed = v["baseSpeed"].AsFloat(baseSpeed);
	loop = v["loop"].AsBool(loop);
	fov = v["fov"].AsFloat(fov);

	const JsonValue& arr = v["points"];

	std::vector<CamPoint>loaded;
	loaded.reserve(arr.Size());


	for (size_t i = 0; i < arr.Size(); i++)
	{
		const JsonValue& o = arr.At(i);

		CamPoint p;
		p.pos = Vec3FromJson(o["pos"], Vector3(0, 0, 0));
		p.speed = o["speed"].AsFloat(1.0f);
		p.roll = o["roll"].AsFloat(0.0f);

		loaded.push_back(p);

	}

	if (loaded.size() < 2) return false;

	points = std::move(loaded);
	return true;

}


