#pragma once
#include<string>
#include<vector>


struct CameraTake;




namespace CameraLibrary
{
	std::vector<std::string>Characters();
	std::vector<std::string>Moves(const std::string& character);
	std::vector<std::string>Takes(const std::string& character,const std::string& move);

	bool Save(const CameraTake& take, std::string* error = nullptr);
	
	bool Load(const std::string& character,
		const std::string& move,
		const std::string&takeName,
		CameraTake&out,
		std::string* error=nullptr);

	bool Remove(const std::string& character, const std::string& move,
		const std::string& takeName);

	

}