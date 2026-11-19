#include	"scenemanager.h"
#include	"SceneClassFactory.h"
#include	"DebugUI.h"

// 登録されているシーンを全て破棄する
void SceneManager::Dispose() 
{
	// 登録されているすべてシーンの終了処理
	for (auto& s : m_scenes) 
	{
		s.second->dispose();
	}

	m_scenes.clear();
	m_currentSceneName.clear();
}

// 一度作ったシーンは残しておき、戻ってきたときはそのまま再開する（init し直さない）。
// カメラの設定や読み込んだモデルが、行き来しても消えないようにするため
void SceneManager::SetCurrentScene(std::string currentscenename)
{
	auto it = m_scenes.find(currentscenename);
	if (it == m_scenes.end())
	{
		auto obj = SceneClassFactory::getInstance().create(currentscenename);
		if (!obj) return;	// 登録されていない名前なら今のシーンのまま
		obj->init();
		it = m_scenes.emplace(currentscenename, std::move(obj)).first;
	}

	m_currentSceneName = currentscenename;
	DebugUI::SetActiveOwner(it->second.get());	// このシーンのツール窓だけを出す
}

void SceneManager::Init()
{
}

void SceneManager::Draw(uint64_t deltatime)
{

	// 現在のシーンを描画
	m_scenes[m_currentSceneName]->draw(deltatime);
}

void SceneManager::Update(uint64_t deltatime)
{
	// 予約されている切り替えを、どのシーンも実行していないこのタイミングで行う
	if (!m_requestedSceneName.empty())
	{
		const std::string next = m_requestedSceneName;
		m_requestedSceneName.clear();
		if (next != m_currentSceneName) SetCurrentScene(next);
	}

	// 現在のシーンを更新
	m_scenes[m_currentSceneName]->update(deltatime);
}