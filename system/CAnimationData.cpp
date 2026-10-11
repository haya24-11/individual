//#include	<cassert>
//#include	<iostream>
//#include	"CAnimationData.h"
//
//const aiScene* CAnimationData::LoadAnimation(const std::string filename, const std::string name)
//{
//	// シーン情報を構築
//	m_Animation[name] = m_importer.ReadFile(
//		filename.c_str(),
//		aiProcess_ConvertToLeftHanded);
//	assert(m_Animation[name]);
//
//	if (m_Animation[name] == nullptr) {
//		std::cout << " animation load error " << filename  << " " << m_importer.GetErrorString();
//	}
//
//	return m_Animation[name];
//}
//
//// 指定した名前のアニメーションデータを取得する
//aiAnimation* CAnimationData::GetAnimation(const char* name, int idx) {
//
//	aiAnimation* animation = m_Animation[name]->mAnimations[idx];
//
//	return animation;
//}


#include	<cassert>
#include	<iostream>
#include	"CAnimationData.h"

const aiScene* CAnimationData::LoadAnimation(const std::string& filename, const std::string& name)
{
	Entry e;
	e.importer = std::make_unique <Assimp::Importer>();

	//シーン情報を構築
	e.scene = e.importer->ReadFile(
		filename.c_str(),
		aiProcess_ConvertToLeftHanded);
	
	if (e.scene == nullptr || e.scene->mNumAnimations == 0)
	{
		std::cout << "animation load error" << filename << " " << e.importer->GetErrorString() << std::endl;
		return nullptr;
	}

	const aiScene* scene = e.scene;
	m_Animation[name] = std::move(e);
	return scene;

}

aiAnimation* CAnimationData::GetAnimation(const std::string& name, int idx)const
{
	auto it = m_Animation.find(name);
	if (it == m_Animation.end()) return nullptr;

	const aiScene* scene = it->second.scene;
	if (idx < 0 || (unsigned int)idx >= scene->mNumAnimations) return nullptr;


	return scene->mAnimations[idx];
}

const aiScene* CAnimationData::GetAiScene(const std::string& name) const 
{
	auto it = m_Animation.find(name);
	return (it == m_Animation.end()) ? nullptr : it->second.scene;
}