// Stand-in for UGroomAsset: only what the writer calls.
#pragma once
#include "CoreMinimal.h"
#include "HairDescription.h"
class UGroomAsset
{
public:
	FHairDescription Description;
	FString Name;
	FHairDescription GetHairDescription() const { return Description; }
	FString GetName() const { return Name; }
};
