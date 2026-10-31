// Groom Alembic Exporter

#include "GroomExportLibrary.h"

#include "GroomAlembicWriter.h"
#include "GroomExportSettings.h"

#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

namespace GroomExportLibraryPrivate
{
	static FGroomExportSettings ToSettings(const FGroomAlembicExportOptions& Options)
	{
		FGroomExportSettings Settings;
		Settings.CoordSystem = Options.bConvertToYUp ? EGroomExportCoordSystem::YUpRightHanded : EGroomExportCoordSystem::UnrealZUp;
		Settings.UnitScale = Options.UnitScale;
		Settings.bSplitByGroup = Options.bSplitByGroup;
		Settings.bExportWidths = Options.bExportWidths;
		Settings.bExportRootUVs = Options.bExportRootUV;
		Settings.bExportGroomAttributes = Options.bExportGroomAttributes;
		Settings.GuideMode = Options.GuideMode;
		Settings.bWritePhysicsSettings = Options.bWritePhysicsSettings;
		return Settings;
	}

	static FGroomAlembicExportOptions ToOptions(const FGroomExportSettings& Settings)
	{
		FGroomAlembicExportOptions Options;
		Options.bConvertToYUp = Settings.CoordSystem == EGroomExportCoordSystem::YUpRightHanded;
		Options.UnitScale = Settings.UnitScale;
		Options.bSplitByGroup = Settings.bSplitByGroup;
		Options.bExportWidths = Settings.bExportWidths;
		Options.bExportRootUV = Settings.bExportRootUVs;
		Options.bExportGroomAttributes = Settings.bExportGroomAttributes;
		Options.GuideMode = Settings.GuideMode;
		Options.bWritePhysicsSettings = Settings.bWritePhysicsSettings;
		return Options;
	}
}

bool UGroomExportBlueprintLibrary::IsExportMenuEntryRegistered()
{
	UToolMenus* Menus = UToolMenus::Get();
	if (!Menus)
	{
		return false;
	}

	UToolMenu* Menu = Menus->FindMenu(TEXT("ContentBrowser.AssetContextMenu.GroomAsset"));
	if (!Menu)
	{
		return false;
	}

	FToolMenuSection* Section = Menu->FindSection(TEXT("GetAssetActions"));
	if (!Section)
	{
		return false;
	}

	// The name given to AddDynamicEntry in GroomExportEditor.cpp. The menu entry it builds
	// ("GroomAsset_ExportToAlembic") only materialises when the menu is generated with a
	// selection, so the dynamic block is what can be checked ahead of time.
	return Section->FindEntry(TEXT("GroomExport_Alembic")) != nullptr;
}

bool UGroomExportBlueprintLibrary::ExportGroomToAlembic(
	UGroomAsset* GroomAsset,
	const FString& Filename,
	FString& OutError,
	int32& OutNumGroups,
	int32& OutNumCurves,
	int32& OutNumPoints,
	bool bConvertToYUp,
	float UnitScale,
	bool bExportGuideCurves)
{
	OutError.Reset();
	OutNumGroups = 0;
	OutNumCurves = 0;
	OutNumPoints = 0;

	FGroomExportSettings Settings;
	Settings.CoordSystem = bConvertToYUp ? EGroomExportCoordSystem::YUpRightHanded : EGroomExportCoordSystem::UnrealZUp;
	Settings.UnitScale = UnitScale;
	Settings.GuideMode = bExportGuideCurves ? EGroomAlembicGuideMode::Mixed : EGroomAlembicGuideMode::Exclude;

	FGroomExportResult Result;
	const bool bSuccess = FGroomAlembicWriter::Export(GroomAsset, Filename, Settings, Result);

	OutNumGroups = Result.NumGroups;
	OutNumCurves = Result.NumCurves;
	OutNumPoints = Result.NumPoints;
	if (!bSuccess)
	{
		OutError = Result.ErrorText.ToString();
	}

	return bSuccess;
}

FGroomAlembicExportReport UGroomExportBlueprintLibrary::ExportGroomToAlembicWithOptions(
	UGroomAsset* GroomAsset,
	const FString& Filename,
	const FGroomAlembicExportOptions& Options)
{
	const FGroomExportSettings Settings = GroomExportLibraryPrivate::ToSettings(Options);

	FGroomExportResult Result;
	FGroomAlembicExportReport Report;
	Report.bSuccess = FGroomAlembicWriter::Export(GroomAsset, Filename, Settings, Result);
	Report.Error = Report.bSuccess ? FString() : Result.ErrorText.ToString();
	Report.NumGroups = Result.NumGroups;
	Report.NumCurveObjects = Result.CurvesObjects.Num();
	Report.NumCurves = Result.NumCurves;
	Report.NumPoints = Result.NumPoints;
	Report.NumGuideCurves = Result.NumGuideCurves;
	Report.NumGuidePoints = Result.NumGuidePoints;
	Report.NumGuideCurvesSkipped = Result.NumGuideCurvesSkipped;
	Report.AlembicFile = Result.AlembicFile;
	Report.PhysicsSettingsFile = Result.PhysicsSettingsFile;
	return Report;
}

FGroomAlembicExportOptions UGroomExportBlueprintLibrary::GetUnrealRoundTripExportOptions()
{
	return GroomExportLibraryPrivate::ToOptions(FGroomExportSettings::MakeUnrealRoundTrip());
}

FGroomAlembicExportOptions UGroomExportBlueprintLibrary::GetBlenderExportOptions()
{
	return GroomExportLibraryPrivate::ToOptions(FGroomExportSettings::MakeBlender());
}
