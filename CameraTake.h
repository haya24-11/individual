#pragma once
#include<string>
#include<vector>
#include "system/commontypes.h"
#include "Json.h"


struct CamPoint
{
	Vector3 pos{};
	float speed = 1.0f;
	float roll = 0.0f;


};

struct CameraTake
{
	std::string character;
	std::string move;
	std::string name;
	std::string p1Model;	//プレイヤー１
	std::string p2Model;	//プレイヤー２

	//スプライン本体
	std::vector<CamPoint> points;
	Vector3 lookat{ 0,0,0 };//注視点
	float baseSpeed = 300.f;//マスター速度
	bool loop		= true;//経路を閉じるか
	float fov		= 45.0f;

	// 作成時の基準位置（＝そのときの Player1 の絶対座標）。
	// 再生には使わない（相対座標さえあれば足りる）が、
	// どういう配置で作ったテイクなのかを残すために記録しておく。
	Vector3 origin{ 0, 0, 0 };

	JsonValue ToJson() const;

	// 読み込み。欠けているキーは今の値（＝既定値）を保つので、
	// 後からキーを増やしても古いファイルがそのまま読める。
	// 制御点が2点未満のときだけ false を返す。

	bool FromJson(const JsonValue& v);

};

