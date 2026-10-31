// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GroomExportTypes.h"
#include "GroomExportLibrary.generated.h"

class UGroomAsset;

/**
 * Scripting entry point for the groom Alembic exporter, so grooms can be exported from
 * Blueprint, Python or a commandlet without going through the Content Browser menu.
 */
UCLASS()
class GROOMEXPORTEDITOR_API UGroomExportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * True when this plugin's "Export Groom to Alembic" entry is present in the Groom Asset
	 * context menu. Only meaningful in a GUI editor session - a commandlet never builds the
	 * Content Browser menus.
	 *
	 * Exists because the menu's sections are not readable from Python, so this is the only
	 * way to check the registration without clicking through the UI by hand.
	 */
	UFUNCTION(BlueprintCallable, Category = "Groom Export")
	static bool IsExportMenuEntryRegistered();

	/**
	 * Writes a groom's strand curves to an Alembic file. Kept as the original 1.0 entry
	 * point; ExportGroomToAlembicWithOptions exposes every option.
	 *
	 * @param GroomAsset          The groom to export.
	 * @param Filename            Absolute path of the .abc file to write.
	 * @param OutError            Empty on success, otherwise why the export failed.
	 * @param OutNumGroups        Number of hair groups written.
	 * @param OutNumCurves        Total curves written, guides included.
	 * @param OutNumPoints        Total points written, guides included.
	 * @param bConvertToYUp       Convert to the Y-up right-handed convention used by Maya
	 *                            and Blender. Leave false to keep Unreal space, which is
	 *                            what round-trips through the groom importer's defaults.
	 * @param UnitScale           Uniform scale on positions and widths. 1.0 keeps centimeters.
	 * @param bExportGuideCurves  Include the curves flagged as simulation guides, mixed with
	 *                            the strands (EGroomAlembicGuideMode::Mixed). False leaves
	 *                            them out (EGroomAlembicGuideMode::Exclude).
	 */
	UFUNCTION(BlueprintCallable, Category = "Groom Export")
	static bool ExportGroomToAlembic(
		UGroomAsset* GroomAsset,
		const FString& Filename,
		FString& OutError,
		int32& OutNumGroups,
		int32& OutNumCurves,
		int32& OutNumPoints,
		bool bConvertToYUp = false,
		float UnitScale = 1.0f,
		bool bExportGuideCurves = true);

	/**
	 * Writes a groom's strand curves to an Alembic file with every option available,
	 * including where the guide curves go and the physics settings sidecar file.
	 *
	 * @param GroomAsset  The groom to export.
	 * @param Filename    Absolute path of the .abc file to write.
	 * @param Options     Export options. See GetBlenderExportOptions / GetUnrealRoundTripExportOptions.
	 * @return            What was written; bSuccess is false and Error set on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Groom Export")
	static FGroomAlembicExportReport ExportGroomToAlembicWithOptions(
		UGroomAsset* GroomAsset,
		const FString& Filename,
		const FGroomAlembicExportOptions& Options);

	/** Options that round-trip back into Unreal unchanged (the defaults). */
	UFUNCTION(BlueprintPure, Category = "Groom Export")
	static FGroomAlembicExportOptions GetUnrealRoundTripExportOptions();

	/**
	 * Options for Blender: Y-up, centimeters, guide curves left out (Blender's Alembic
	 * importer ignores groom_guide and would render them as extra hair), physics settings
	 * written next to the file.
	 */
	UFUNCTION(BlueprintPure, Category = "Groom Export")
	static FGroomAlembicExportOptions GetBlenderExportOptions();
};
