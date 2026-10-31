#pragma once
#include "CoreMinimal.h"
class IFileManager
{
public:
	static IFileManager& Get() { static IFileManager Instance; return Instance; }
	bool MakeDirectory(const char* Path, bool) { std::error_code E; std::filesystem::create_directories(Path, E); return !E; }
};
