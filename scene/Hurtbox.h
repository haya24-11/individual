#pragma once
#include <string>
#include <vector>
#include "../system/commontypes.h"

// やられ判定の形。ボーン2点を囲む長方形に、余白とずれを足したもの。
// 2D 格闘ゲームと同じく、戦う平面（XY）だけで考える。
// キャラ全体で共通（どの技でも同じ形）。対戦画面とモーション編集の両方がこれを使う
struct HurtboxDef
{
	std::string from;			// 囲むボーン（始点）
	std::string to;				// 囲むボーン（終点）
	float padX = 10.0f;			// 横の余白（片側）
	float padY = 10.0f;			// 縦の余白（片側）
	float offX = 0.0f;			// 中心のずれ。+ がキャラの前方
	float offY = 0.0f;			// 中心のずれ。+ が上
};

// 平面上の長方形（ワールド座標）
struct HurtRect
{
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	float cx() const { return (x0 + x1) * 0.5f; }
	float cy() const { return (y0 + y1) * 0.5f; }
	float w()  const { return x1 - x0; }
	float h()  const { return y1 - y0; }
};

namespace Hurtbox
{
	// 今の形（最初に呼んだときに HurtboxConfig.json を読む。無ければ初期値）
	const std::vector<HurtboxDef>& Defs();

	// 形を書き換える（呼んだら「保存していない変更あり」になる）
	std::vector<HurtboxDef>& Edit();
	bool IsDirty();

	bool Save(std::string* error = nullptr);	// HurtboxConfig.json に書く
	bool Reload(std::string* error = nullptr);	// ファイルから読み直す（変更は捨てる）
	void ResetDefaults();						// 最初の形（コードの初期値）に戻す

	// a, b はボーンのワールド位置。scale はキャラの大きさ、facing は +1（右向き）/ -1（左向き）
	HurtRect Compute(const HurtboxDef& d, const Vector3& a, const Vector3& b, float scale, float facing);

	// ワールド行列からキャラの向きを求める（+1 = +X を向いている）
	float FacingOf(const Matrix4x4& world);
}
