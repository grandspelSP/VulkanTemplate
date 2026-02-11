#pragma once
#include <memory>
#include <vector>
#include <filesystem>

//---------------------------------------------------------------------------
class FileLoader;
std::unique_ptr<FileLoader>& getFileLoader();

//---------------------------------------------------------------------------
class FileLoader
{
public:
	bool Load(std::filesystem::path filePath, std::vector<char>& fileData);
};
//---------------------------------------------------------------------------