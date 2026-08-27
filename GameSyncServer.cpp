#include "GameSyncServer.h"
#include "system/meshmanager.h"
#include "system/CStaticMesh.h"
#include "system/utility.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace {
	constexpr wchar_t   kMapName[] = L"Local\\GM31_CameraSync";
	constexpr DWORD     kMapSize = 512;
	constexpr unsigned  kMagic = 0x314D4347;	// "GCM1"

	// ツール側(GameSync.cs)と一致させること
	constexpr int kOffMagic = 0;
	constexpr int kOffVersion = 4;
	constexpr int kOffFrame = 8;
	constexpr int kOffFlags = 12;
	constexpr int kOffCamPos = 16;
	constexpr int kOffCamLook = 28;
	constexpr int kOffCamUp = 40;
	constexpr int kOffFov = 52;
	constexpr int kOffP1Pos = 56;
	constexpr int kOffP1RotY = 68;
	constexpr int kOffP1Scale = 72;
	constexpr int kOffP2Pos = 76;
	constexpr int kOffP2RotY = 88;
	constexpr int kOffP2Scale = 92;
	constexpr int kOffP1Mesh = 96;
	constexpr int kOffP2Mesh = 160;
	constexpr int kMeshIdBytes = 64;

	void PutU32(unsigned char* p, int off, unsigned v) { std::memcpy(p + off, &v, 4); }
	void PutF32(unsigned char* p, int off, float v) { std::memcpy(p + off, &v, 4); }

	void PutVec(unsigned char* p, int off, const Vector3& v) {
		PutF32(p, off, v.x); PutF32(p, off + 4, v.y); PutF32(p, off + 8, v.z);
	}

	// UTF-8のままコピーし、必ずNUL終端にする
	void PutStr(unsigned char* p, int off, const std::string& s, int max) {
		std::memset(p + off, 0, max);
		int n = (int)s.size();
		if (n > max - 1) n = max - 1;
		std::memcpy(p + off, s.data(), n);
	}
}

bool GameSyncServer::Open()
{
	m_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
		PAGE_READWRITE, 0, kMapSize, kMapName);
	if (m_map == nullptr) return false;

	m_view = (unsigned char*)MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, kMapSize);
	if (m_view == nullptr) { CloseHandle(m_map); m_map = nullptr; return false; }

	std::memset(m_view, 0, kMapSize);
	PutU32(m_view, kOffMagic, kMagic);
	PutU32(m_view, kOffVersion, 1);

	return true;
}

void GameSyncServer::Close()
{
	if (m_view) { UnmapViewOfFile(m_view); m_view = nullptr; }
	if (m_map) { CloseHandle(m_map); m_map = nullptr; }
}

void GameSyncServer::Publish(const Camera& cam,
	const Vector3& p1Pos, float p1RotY, float p1Scale, const std::string& p1Mesh,
	const Vector3& p2Pos, float p2RotY, float p2Scale, const std::string& p2Mesh,
	bool recording, bool playing)
{
	if (m_view == nullptr) return;

	PutVec(m_view, kOffCamPos, cam.GetPosition());
	PutVec(m_view, kOffCamLook, cam.GetLookat());
	PutVec(m_view, kOffCamUp, cam.GetUP());
	PutF32(m_view, kOffFov, 45.0f);		// Camera が45度固定のため

	PutVec(m_view, kOffP1Pos, p1Pos);
	PutF32(m_view, kOffP1RotY, p1RotY);
	PutF32(m_view, kOffP1Scale, p1Scale);

	PutVec(m_view, kOffP2Pos, p2Pos);
	PutF32(m_view, kOffP2RotY, p2RotY);
	PutF32(m_view, kOffP2Scale, p2Scale);

	PutStr(m_view, kOffP1Mesh, p1Mesh, kMeshIdBytes);
	PutStr(m_view, kOffP2Mesh, p2Mesh, kMeshIdBytes);

	unsigned flags = (recording ? 1u : 0u) | (playing ? 2u : 0u);
	PutU32(m_view, kOffFlags, flags);

	// frame は最後に書く（中途半端な状態を読ませないため）
	PutU32(m_view, kOffFrame, ++m_frame);
}

// メッシュを .pvm（位置＋法線＋インデックス）で書き出す
bool GameSyncServer::ExportPreviewMesh(const std::string& meshId)
{
	CStaticMesh* mesh = MeshManager::getMesh<CStaticMesh>(meshId);
	if (mesh == nullptr) return false;

	const std::vector<VERTEX_3D>& verts = mesh->GetVertices();
	const std::vector<unsigned int>& idx = mesh->GetIndices();
	if (verts.empty() || idx.empty()) return false;

	namespace fs = std::filesystem;
	fs::path dir = fs::path(L"CameraLibrary") / L"_models";

	std::error_code ec;
	fs::create_directories(dir, ec);

	fs::path file = dir / (utility::utf8_to_wide_winapi(meshId) + L".pvm");

	std::ofstream ofs(file, std::ios::binary);
	if (!ofs) return false;

	unsigned vcount = (unsigned)verts.size();
	unsigned icount = (unsigned)idx.size();

	ofs.write("PVM1", 4);
	ofs.write((const char*)&vcount, 4);
	ofs.write((const char*)&icount, 4);

	for (const VERTEX_3D& v : verts) {
		float p[3] = { v.Position.x, v.Position.y, v.Position.z };
		ofs.write((const char*)p, sizeof(p));
	}
	for (const VERTEX_3D& v : verts) {
		float n[3] = { v.Normal.x, v.Normal.y, v.Normal.z };
		ofs.write((const char*)n, sizeof(n));
	}
	ofs.write((const char*)idx.data(), sizeof(unsigned int) * icount);

	return true;
}