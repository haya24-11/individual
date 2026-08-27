#pragma once
#include<Windows.h>
#include<string>
#include "system/commontypes.h"
#include "system/Camera.h"


class GameSyncServer
{
public:
	bool Open();		// 共有メモリを作る（失敗しても致命的ではない）
	void Close();

	// 1フレーム分の状態を書き込む
	void Publish(const Camera& cam,
		const Vector3& p1Pos, float p1RotY, float p1Scale, const std::string& p1Mesh,
		const Vector3& p2Pos, float p2RotY, float p2Scale, const std::string& p2Mesh,
		bool recording, bool playing);

	// プレビュー用メッシュ(.pvm)を CameraLibrary/_models/ へ書き出す
	static bool ExportPreviewMesh(const std::string& meshId);

private:
	HANDLE m_map = nullptr;
	unsigned char* m_view = nullptr;
	unsigned int m_frame = 0;
};