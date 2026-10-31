// The real physics writer needs UE reflection and JSON; the harness only checks it is called.
#include "GroomPhysicsSettingsWriter.h"
#include "GroomAlembicWriter.h"
#include <cstdio>
FString FGroomPhysicsSettingsWriter::GetSidecarFilename(const FString& InAlembicFilename) { return InAlembicFilename + FString(".physics.json"); }
bool FGroomPhysicsSettingsWriter::Write(const UGroomAsset*, const FString& InFilename, const FGroomExportSettings&, const FGroomExportResult& InResult, FText&)
{
	std::printf("     physics stub called: %s (after archive closed, %d objects)\n", *InFilename, (int)InResult.CurvesObjects.Num());
	return true;
}
