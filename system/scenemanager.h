#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <cstdint>
#include "IScene.h"
#include "noncopyable.h"

// 前方宣言
class IScene;

class SceneManager : NonCopyable{

	static inline std::unordered_map<std::string, std::unique_ptr<IScene>> m_scenes{};
	static inline std::string m_currentSceneName{};
	static inline std::string m_requestedSceneName{};	// 次のフレームの頭で切り替える先（空なら切り替えない）

public:
	static void SetCurrentScene(std::string);

	// シーンの切り替えを予約する。ImGui のボタンやシーンの update の途中から呼んでよい。
	// その場で切り替えると「実行中のシーン自身」を壊す恐れがあるので、次の Update の頭で行う
	static void RequestScene(const std::string& name) { m_requestedSceneName = name; }
	static const std::string& GetCurrentSceneName() { return m_currentSceneName; }
	static void Dispose();
	static void Init();
	static void Update(uint64_t deltatime);
	static void Draw(uint64_t deltatime);
	static IScene* GetCurrentScene() { return m_scenes[m_currentSceneName].get(); }

};
