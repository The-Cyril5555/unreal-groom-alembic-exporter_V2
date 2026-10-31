// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportTypes.h"

/** Target coordinate convention for the written Alembic file. */
enum class EGroomExportCoordSystem : uint8
{
	/**
	 * Write positions exactly as they are stored in the Groom Asset (Unreal space:
	 * Z-up, left handed, centimeters). This is the round-trip safe option: re-importing
	 * the file with the default groom import conversion settings (Rotation 0,0,0 /
	 * Scale 1,1,1) reproduces the original groom.
	 */
	UnrealZUp,

	/**
	 * Convert to the Y-up right handed convention used by Maya / Blender / Houdini:
	 * (X, Y, Z)_unreal -> (X, Z, Y)_alembic.
	 *
	 * The axis swap has determinant -1, which is exactly what is required when moving
	 * between a left handed and a right handed basis: the geometry keeps its shape and
	 * is NOT mirrored.
	 *
	 * Note: to re-import such a file into Unreal you must set the groom importer's
	 * conversion settings to Rotation (90, 0, 0) / Scale (1, 1, 1) - it will not
	 * round-trip with the default identity settings.
	 */
	YUpRightHanded
};

/** User facing options for a single groom -> .abc export. */
struct FGroomExportSettings
{
	/** Coordinate convention of the written file. */
	EGroomExportCoordSystem CoordSystem = EGroomExportCoordSystem::UnrealZUp;

	/**
	 * Uniform scale applied to positions (and widths) on the way out.
	 * Groom assets store centimeters, and UE's groom importer treats Alembic units as
	 * centimeters too (its UNIT_TO_CM is 1), so 1.0 keeps the file in centimeters.
	 * Use 0.01 to write meters.
	 */
	float UnitScale = 1.0f;

	/** Write one OCurves object per hair group instead of a single OCurves for the whole groom. */
	bool bSplitByGroup = true;

	/** Write the per-point (or per-curve) width parameter, when the groom has widths. */
	bool bExportWidths = true;

	/** Write the root UVs as the OCurves UV parameter (uniform scope), when present. */
	bool bExportRootUVs = true;

	/** Write groom_group_id / groom_id / groom_guide / groom_clumpid arbGeomParams. */
	bool bExportGroomAttributes = true;

	/**
	 * Where the curves flagged as simulation guides (groom_guide == 1) go: mixed with the
	 * strands (round trip), in their own curves objects, or left out.
	 */
	EGroomAlembicGuideMode GuideMode = EGroomAlembicGuideMode::Mixed;

	/** Also write the groom's simulation settings to <file>.physics.json next to the .abc. */
	bool bWritePhysicsSettings = false;

	/** Settings that round-trip back into Unreal unchanged (the defaults). */
	static FGroomExportSettings MakeUnrealRoundTrip()
	{
		return FGroomExportSettings();
	}

	/**
	 * Settings for Blender: Y-up, centimeters, no guide curves (Blender would render them
	 * as extra hair), and the physics settings written next to the file.
	 */
	static FGroomExportSettings MakeBlender()
	{
		FGroomExportSettings Settings;
		Settings.CoordSystem = EGroomExportCoordSystem::YUpRightHanded;
		Settings.GuideMode = EGroomAlembicGuideMode::Exclude;
		Settings.bWritePhysicsSettings = true;
		return Settings;
	}
};
