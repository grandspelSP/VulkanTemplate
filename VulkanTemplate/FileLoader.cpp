#include "FileLoader.h"

#include <fstream>

//---------------------------------------------------------------------------
static std::unique_ptr<FileLoader> fileLoader = nullptr;
std::unique_ptr<FileLoader>& getFileLoader()
{
    if (fileLoader == nullptr)
    {
        fileLoader = std::make_unique<FileLoader>();
    }
    return fileLoader;
}


//---------------------------------------------------------------------------
bool FileLoader::Load(std::filesystem::path filePath, std::vector<char>& fileData)
{
    if (std::filesystem::exists(filePath))
    {
        std::ifstream infile(filePath, std::ios::binary);
        if (infile)
        {
            auto size = infile.seekg(0, std::ios::end).tellg();
            fileData.resize(size);
            infile.seekg(0, std::ios::beg).read(fileData.data(), size);
            return true;
        }
    }
    filePath = std::filesystem::path("../") / filePath;
    if (std::filesystem::exists(filePath))
    {
        std::ifstream infile(filePath, std::ios::binary);
        if (infile)
        {
            auto size = infile.seekg(0, std::ios::end).tellg();
            fileData.resize(size);
            infile.seekg(0, std::ios::beg).read(fileData.data(), size);
            return true;
        }
    }
    return false;
}
//---------------------------------------------------------------------------