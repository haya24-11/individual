#include	"scenemanager.h"
#include	"SceneClassFactory.h"

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

void SceneManager::SetCurrentScene(std::string currentscenename) 
{
	m_currentSceneName = currentscenename;
	auto obj = SceneClassFactory::getInstance().create(currentscenename);
	obj->init();
	m_scenes[m_currentSceneName] = std::move(obj);
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
	// 現在のシーンを更新
	m_scenes[m_currentSceneName]->update(deltatime);
}