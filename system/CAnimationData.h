//#pragma once
//#include	<assimp\Importer.hpp>
//#include	<assimp\scene.h>
//#include	<assimp\postprocess.h>
//#include	<assimp/cimport.h>
//#include	<unordered_map>
//#include	"NonCopyable.h"
//
//class CAnimationData : NonCopyable{
//	// このアニメーションのパス名
//	std::string m_filename;
//
//	// アニメーションデータ格納辞書（キーはモーション名）
//	std::unordered_map<std::string, const aiScene*> m_Animation;
//
//	// importer解放される際　シーンも解放されるのでメンバ変数にしてる
//	Assimp::Importer m_importer;	// ボーン情報
//public:
//	// アニメーションデータ読み込み
//	const aiScene* LoadAnimation(const std::string filename, const std::string name);
//
//	// 指定した名前のアニメーションを取得する
//	aiAnimation* GetAnimation(const char* name, int idx);
//
//	// アニメーションデータが格納されているａｉＳｃｅｎｅを獲得する
//	const aiScene* GetAiScene(std::string name) 
//	{ 
//		return m_Animation[name]; 
//	}
//};

#pragma once
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include	<assimp\postprocess.h>
#include	<assimp/cimport.h>
#include	<memory>
#include	<string>
#include	<unordered_map>
#include	"NonCopyable.h"

class CAnimationData:NonCopyable
{

	struct Entry
	{
		std::unique_ptr<Assimp::Importer>importer;//シーンの所有者
		const aiScene* scene = nullptr;			  //importerが持つシーン
	};

	std::unordered_map<std::string, Entry>m_Animation;

public:

	const aiScene* LoadAnimation(const std::string& filename, const std::string& name);


	bool Contains(const std::string& name)const
	{
		return m_Animation.find(name) != m_Animation.end();
	}

	//指定した名前のアニメーションを取得する（未読み込み・範囲外なら　nullptr)
	aiAnimation* GetAnimation(const std::string& name, int idx = 0)const;

	const aiScene* GetAiScene(const std::string& name)const;




};

