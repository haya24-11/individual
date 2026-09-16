#include"CameraLibrary.h"
#include "CameraTake.h"
#include "system/utility.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iterator>


namespace
{
	namespace fs = std::filesystem;
	fs::path ToPath(const std::string& utf8)
	{
		return	fs::path(utility::utf8_to_wide_winapi(utf8));
	}

	fs::path RootPath()
	{
		return ToPath("CameraLibrary");
	}

	fs::path CharPath(const std::string& character)
	{
		return RootPath() / ToPath(character);
	}

	fs::path MovePath(const std::string& character,const std::string& move)
	{
		return CharPath(character) / ToPath(move);
	}

	fs::path TakePath(const std::string& character, const std::string& move,
		const std::string& takeName)
	{
		return MovePath(character, move) / ToPath(takeName + ".json");
	}

	// 指定フォルダ直下のサブフォルダ名を UTF-8 で列挙する
	std::vector<std::string> ListDirs(const fs::path& dir)
	{
		std::vector<std::string> out;
		try
		{
			if (!fs::exists(dir)) return out;

			for (const auto& e : fs::directory_iterator(dir))
			{
				if (!e.is_directory()) continue;
				out.push_back(utility::PathToUtf8String(e.path().filename()));
			}
		}
		catch (const std::exception&)
		{
			// フォルダが途中で消えた等。空のまま返して落とさない。
		}

		std::sort(out.begin(), out.end());
		return out;
	}
}

std::vector<std::string>CameraLibrary::Characters()
{
	return ListDirs(RootPath());
}

std::vector<std::string>CameraLibrary::Moves(const std::string& character)
{
	if (character.empty())return {};

	try { return ListDirs(CharPath(character)); }
	catch (const std::exception&) { return{}; }
}

std::vector<std::string>CameraLibrary::Takes(const std::string& character,const std::string&move)
{
	if (character.empty() || move.empty()) return{};

	std::vector<std::string>out;
	try 
	{
		const fs::path dir = MovePath(character, move);
		if (!fs::exists(dir)) return out;

		for (const auto& e : fs::directory_iterator(dir))
		{
			if (!e.is_regular_file())continue;

			out.push_back(utility::PathToUtf8String(e.path().stem()));
		}
	}
	catch (const std::exception&)
	{ 
	}
		std::sort(out.begin(), out.end());
		return out;
	
}

bool CameraLibrary::Save(const CameraTake& take, std::string* error)
{
	if (take.character.empty()) { if (error)*error = "キャラクター名が空です。"; return false; }
	if (take.move.empty()) { if (error)*error = "技名が空です"; return false; }
	if (take.name.empty()) { if (error)*error = "テイク名が空です"; return false; }


	if (take.points.size() < 4) { if (error)*error = "制御点が4点未満です"; return false; }

	try
	{
		fs::create_directories(MovePath(take.character, take.move));	// 既にあっても失敗しない

		const fs::path file = TakePath(take.character, take.move, take.name);

		// fs::path を渡すと内部でワイド文字のまま開かれる（日本語パスでも安全）。
		// binary にして、書いた通りの改行で保存する。
		std::ofstream ofs(file, std::ios::binary);
		if (!ofs) { if (error) *error = "ファイルを開けません"; return false; }

		const std::string text = take.ToJson().Dump(2);
		ofs.write(text.data(), static_cast<std::streamsize>(text.size()));

		if (!ofs) { if (error) *error = "書き込みに失敗しました"; return false; }
	}
	catch (const std::exception& e)
	{
		if (error) *error = e.what();
		return false;
	}

	return true;
}

bool CameraLibrary::Load(const std::string& character,
	const std::string& move,
	const std::string& takeName,
	CameraTake& out,
	std::string* error)
{
	try
	{
		std::ifstream ifs(TakePath(character, move, takeName), std::ios::binary);
		if (!ifs) { if (error) *error = "ファイルが見つかりません"; return false; }

		const std::string text((std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>());

		JsonValue root;
		std::string perr;
		if (!JsonValue::Parse(text, root, &perr))
		{
			if (error) *error = "JSONとして読めません: " + perr;
			return false;
		}

		CameraTake loaded;
		if (!loaded.FromJson(root))
		{
			if (error) *error = "テイクとして成立しません（制御点が足りません）";
			return false;
		}

		// フォルダ名・ファイル名を正とする。
		// ファイルを移動・改名しても、実際の置き場所と食い違わないようにする。
		loaded.character = character;
		loaded.move = move;
		loaded.name = takeName;

		out = std::move(loaded);
	}
	catch (const std::exception& e)
	{
		if (error) *error = e.what();
		return false;
	}

	return true;
}



bool CameraLibrary::Remove(const std::string& character,
	const std::string& move,
	const std::string& takeName)
{
	try
	{
		std::error_code ec;
		return fs::remove(TakePath(character, move, takeName), ec);
	}
	catch (const std::exception&)
	{
		return false;
	}
}