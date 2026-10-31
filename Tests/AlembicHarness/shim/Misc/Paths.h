#pragma once
#include "CoreMinimal.h"
struct FPaths
{
	static FString ConvertRelativePathToFull(const FString& P) { return FString(std::filesystem::absolute(P.S).string()); }
	static FString GetPath(const FString& P) { return FString(std::filesystem::path(P.S).parent_path().string()); }
};
