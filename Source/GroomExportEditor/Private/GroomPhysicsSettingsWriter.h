// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportSettings.h"

class UGroomAsset;
struct FGroomExportResult;

/**
 * Writes a groom's simulation settings to a small JSON file next to its .abc, so they can
 * be reproduced in another DCC (Alembic has no place for them).
 *
 * The values are read through UE reflection rather than hand-picked fields, so whatever
 * the engine version stores is written, under the engine's own property names (as
 * FJsonObjectConverter::StandardizeCase writes them: first letter lower-cased, "ID" -> "Id"):
 *
 *   groups[i].physics        FHairGroupsPhysics of hair group i: solver settings, external
 *                            forces (gravity, air drag), bend / stretch stiffness and
 *                            damping, collisions (friction, viscosity, radius), strand size.
 *   groups[i].interpolation  FHairGroupsInterpolation: guide density, interpolation and
 *                            rigging settings (how the guides drive the strands).
 *   groups[i].info           FHairGroupInfo: curve / guide counts as Unreal built them.
 *
 * Values are copied verbatim, in Unreal units (centimeters, Z-up, cm/s^2 for gravity),
 * whatever coordinate system and unit scale the .abc was written with.
 */
class FGroomPhysicsSettingsWriter
{
public:
	/** "<path>/<name>.abc" -> "<path>/<name>.physics.json". */
	static FString GetSidecarFilename(const FString& InAlembicFilename);

	/** Returns false and fills OutError on failure. */
	static bool Write(
		const UGroomAsset* InGroomAsset,
		const FString& InFilename,
		const FGroomExportSettings& InSettings,
		const FGroomExportResult& InExportResult,
		FText& OutError);
};
