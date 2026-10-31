// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportSettings.h"

class UGroomAsset;

/** One OCurves object of the written archive. */
struct FGroomExportCurvesObject
{
	/** Alembic object name, e.g. "group_0" or "guides_group_0". */
	FString Name;
	/** Hair group the curves belong to (0 when the groom is not split by group). */
	int32 GroupID = 0;
	/** True for an object that holds only guide curves (Separate guide mode). */
	bool bGuidesOnly = false;
	int32 NumCurves = 0;
	int32 NumPoints = 0;
	/** Guide curves inside this object (all of them for a guides-only object). */
	int32 NumGuideCurves = 0;
};

/** Summary of what a single export produced, for logging / user feedback. */
struct FGroomExportResult
{
	/** Distinct hair groups written. */
	int32 NumGroups = 0;
	/** Every curve / point written, guides included. */
	int32 NumCurves = 0;
	int32 NumPoints = 0;
	/** Guide curves / points written to the file, mixed in or in their own objects. */
	int32 NumGuideCurves = 0;
	int32 NumGuidePoints = 0;
	/** Guide curves left out (Exclude guide mode). */
	int32 NumGuideCurvesSkipped = 0;
	bool  bWroteWidths = false;
	bool  bWroteRootUVs = false;

	/** Every OCurves object written, in file order. */
	TArray<FGroomExportCurvesObject> CurvesObjects;

	/** Absolute path of the .abc written. */
	FString AlembicFile;
	/** Absolute path of the physics settings file, when one was written. */
	FString PhysicsSettingsFile;

	/** Populated when the export failed. */
	FText ErrorText;
};

/**
 * Writes the strand (curve) data of a UGroomAsset to an Alembic file that matches the
 * schema UE's own groom Alembic importer expects (see Engine/Plugins/Importers/
 * AlembicHairImporter/.../AlembicHairTranslator.cpp).
 *
 * Layout produced:
 *
 *   /groom                     OXform, identity, carries the groom-scope user properties
 *     /group_0                 OCurves, linear / non-periodic / no basis
 *     /group_1                 OCurves
 *     ...
 *     /guides_group_0          OCurves, guide curves only (EGroomAlembicGuideMode::Separate)
 *     /guides_group_1          ...
 *
 * Each OCurves carries:
 *   - P            : point positions
 *   - nVertices    : point count per curve
 *   - width        : per-point (vertex scope) or per-curve (uniform scope), when available
 *   - uv           : root UV, uniform scope, when available
 *   - arbGeomParams: every "groom_*" strand/vertex attribute of the source groom
 *                    (groom_group_id, groom_id, groom_guide, groom_clumpid, groom_color, ...)
 */
class FGroomAlembicWriter
{
public:
	/**
	 * Editor-only: requires the groom's source FHairDescription. Returns false on failure.
	 * When InSettings.bWritePhysicsSettings is set, also writes the physics sidecar file
	 * (see FGroomPhysicsSettingsWriter).
	 */
	static bool Export(
		const UGroomAsset* InGroomAsset,
		const FString& InFilename,
		const FGroomExportSettings& InSettings,
		FGroomExportResult& OutResult);
};
