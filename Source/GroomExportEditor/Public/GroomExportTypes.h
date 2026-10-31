// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportTypes.generated.h"

/**
 * What to do with the curves a groom flags as simulation guides (groom_guide != 0).
 *
 * Guides drive the physics; they are not meant to be rendered. Unreal tells them apart
 * through the groom_guide attribute, but most DCC importers (Blender's included) ignore
 * that attribute, so guides left in the same curves object as the strands get rendered
 * as extra hair.
 */
UENUM(BlueprintType)
enum class EGroomAlembicGuideMode : uint8
{
	/**
	 * Guides stay in the same curves object as the strands, flagged by groom_guide.
	 * This is the original behaviour and the byte-identical round trip into Unreal.
	 */
	Mixed UMETA(DisplayName = "Keep with the strands"),

	/**
	 * Guides go into their own curves objects (guides_group_N, or "guides" when the
	 * groom is not split by group), always flagged with groom_guide = 1. Strands and
	 * guides can then be shown, hidden or deleted independently in any DCC.
	 */
	Separate UMETA(DisplayName = "Separate objects"),

	/** Guides are not written at all: the file holds the render strands only. */
	Exclude UMETA(DisplayName = "Leave out"),
};

/** Export options for scripting (Blueprint / Python). Mirrors the options dialog. */
USTRUCT(BlueprintType)
struct GROOMEXPORTEDITOR_API FGroomAlembicExportOptions
{
	GENERATED_BODY()

	/**
	 * Convert to the Y-up right-handed convention used by Maya and Blender. Leave false
	 * to keep Unreal space, which is what round-trips through the groom importer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bConvertToYUp = false;

	/** Uniform scale on positions and widths. 1.0 keeps centimeters, 0.01 writes meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export", meta = (ClampMin = "0.000001"))
	float UnitScale = 1.0f;

	/** One curves object per hair group instead of a single one for the whole groom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bSplitByGroup = true;

	/** Write the strand widths, when the groom has them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bExportWidths = true;

	/** Write the per-strand root UVs, when the groom has them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bExportRootUV = true;

	/** Write the remaining groom_* attributes (needed for a faithful round trip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bExportGroomAttributes = true;

	/** Where the simulation guide curves go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	EGroomAlembicGuideMode GuideMode = EGroomAlembicGuideMode::Mixed;

	/**
	 * Also write <file>.physics.json next to the .abc, holding the groom's per-group
	 * simulation settings (stiffness, damping, gravity, collisions, ...).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom Export")
	bool bWritePhysicsSettings = false;
};

/** What an export produced. Returned by UGroomExportBlueprintLibrary::ExportGroomToAlembicWithOptions. */
USTRUCT(BlueprintType)
struct GROOMEXPORTEDITOR_API FGroomAlembicExportReport
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	bool bSuccess = false;

	/** Empty on success, otherwise why the export failed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	FString Error;

	/** Distinct hair groups written. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumGroups = 0;

	/** OCurves objects written (strand objects plus, in Separate mode, guide objects). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumCurveObjects = 0;

	/** Every curve written, guides included. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumCurves = 0;

	/** Every point written, guides included. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumPoints = 0;

	/** Guide curves written to the file (mixed in, or in their own objects). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumGuideCurves = 0;

	/** Points of the guide curves written to the file. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumGuidePoints = 0;

	/** Guide curves left out (Exclude mode). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	int32 NumGuideCurvesSkipped = 0;

	/** Absolute path of the .abc written. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	FString AlembicFile;

	/** Absolute path of the physics settings file, when one was written. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom Export")
	FString PhysicsSettingsFile;
};
