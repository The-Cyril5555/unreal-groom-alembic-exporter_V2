// Groom Alembic Exporter

#include "GroomPhysicsSettingsWriter.h"

#include "GroomAlembicWriter.h"
#include "GroomAsset.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "GroomExport"

namespace GroomPhysicsSettingsPrivate
{
	/** A per-group array of UGroomAsset copied into groups[i].<JsonKey>. */
	struct FGroupArraySource
	{
		const TCHAR* PropertyName;
		const TCHAR* JsonKey;
		/** Fail the export when the engine has no such array. */
		bool bRequired;
	};

	// Looked up by name through reflection: the C++ accessors of UGroomAsset moved
	// between engine versions, the reflected property names did not.
	static const FGroupArraySource GGroupArraySources[] =
	{
		{ TEXT("HairGroupsPhysics"),       TEXT("physics"),       true  },
		{ TEXT("HairGroupsInterpolation"), TEXT("interpolation"), false },
		{ TEXT("HairGroupsInfo"),          TEXT("info"),          false },
	};

	/** Asset-wide properties that affect how guides drive the strands, copied when present. */
	static const TCHAR* GAssetPropertyNames[] =
	{
		TEXT("EnableGlobalInterpolation"),
		TEXT("HairInterpolationType"),
		TEXT("RiggedSkeletalMesh"),
	};

	static const TCHAR* GuideModeToString(EGroomAlembicGuideMode Mode)
	{
		switch (Mode)
		{
		case EGroomAlembicGuideMode::Separate: return TEXT("separate");
		case EGroomAlembicGuideMode::Exclude:  return TEXT("exclude");
		default:                               return TEXT("mixed");
		}
	}

	/** Which objects of the .abc were written, so a DCC-side script can match them up. */
	static TSharedRef<FJsonObject> MakeExportObject(const FGroomExportSettings& Settings, const FGroomExportResult& Result)
	{
		TSharedRef<FJsonObject> Export = MakeShared<FJsonObject>();
		Export->SetStringField(TEXT("alembic_file"), FPaths::GetCleanFilename(Result.AlembicFile));
		Export->SetStringField(TEXT("coordinate_system"),
			Settings.CoordSystem == EGroomExportCoordSystem::YUpRightHanded ? TEXT("y_up") : TEXT("unreal_z_up"));
		Export->SetNumberField(TEXT("unit_scale"), Settings.UnitScale);
		Export->SetStringField(TEXT("guide_mode"), GuideModeToString(Settings.GuideMode));
		Export->SetNumberField(TEXT("num_curves"), Result.NumCurves);
		Export->SetNumberField(TEXT("num_points"), Result.NumPoints);
		Export->SetNumberField(TEXT("num_guide_curves"), Result.NumGuideCurves);
		Export->SetNumberField(TEXT("num_guide_curves_left_out"), Result.NumGuideCurvesSkipped);

		TArray<TSharedPtr<FJsonValue>> Objects;
		for (const FGroomExportCurvesObject& Object : Result.CurvesObjects)
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), Object.Name);
			Entry->SetNumberField(TEXT("group_id"), Object.GroupID);
			Entry->SetBoolField(TEXT("guides_only"), Object.bGuidesOnly);
			Entry->SetNumberField(TEXT("num_curves"), Object.NumCurves);
			Entry->SetNumberField(TEXT("num_points"), Object.NumPoints);
			Entry->SetNumberField(TEXT("num_guide_curves"), Object.NumGuideCurves);
			Objects.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Export->SetArrayField(TEXT("curves_objects"), Objects);
		return Export;
	}
} // namespace GroomPhysicsSettingsPrivate

FString FGroomPhysicsSettingsWriter::GetSidecarFilename(const FString& InAlembicFilename)
{
	// GetBaseFilename (unlike ChangeExtension) also handles a path without extension, so
	// the sidecar can never end up with the .abc's own name.
	return FPaths::GetBaseFilename(InAlembicFilename, /*bRemovePath*/ false) + TEXT(".physics.json");
}

bool FGroomPhysicsSettingsWriter::Write(
	const UGroomAsset* InGroomAsset,
	const FString& InFilename,
	const FGroomExportSettings& InSettings,
	const FGroomExportResult& InExportResult,
	FText& OutError)
{
	using namespace GroomPhysicsSettingsPrivate;

	if (!InGroomAsset)
	{
		OutError = LOCTEXT("PhysicsNullGroom", "No groom asset supplied.");
		return false;
	}

	const UClass* GroomClass = InGroomAsset->GetClass();

	TArray<TSharedPtr<FJsonObject>> Groups;
	TArray<TSharedPtr<FJsonValue>> NotFound;

	for (const FGroupArraySource& Source : GGroupArraySources)
	{
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(GroomClass->FindPropertyByName(Source.PropertyName));
		const FStructProperty* ElementProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
		if (!ElementProperty)
		{
			if (Source.bRequired)
			{
				OutError = FText::Format(
					LOCTEXT("PhysicsArrayMissingFmt",
						"this engine version's Groom Asset has no '{0}' array, so its simulation settings cannot be read."),
					FText::FromString(Source.PropertyName));
				return false;
			}
			NotFound.Add(MakeShared<FJsonValueString>(FString(Source.PropertyName)));
			continue;
		}

		const UScriptStruct* ElementStruct = ElementProperty->Struct;
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(InGroomAsset));

		for (int32 GroupIndex = 0; GroupIndex < ArrayHelper.Num(); ++GroupIndex)
		{
			while (Groups.Num() <= GroupIndex)
			{
				TSharedPtr<FJsonObject> Group = MakeShared<FJsonObject>();
				Group->SetNumberField(TEXT("group_index"), Groups.Num());
				Groups.Add(Group);
			}

			TSharedRef<FJsonObject> Element = MakeShared<FJsonObject>();
			if (FJsonObjectConverter::UStructToJsonObject(ElementStruct, ArrayHelper.GetRawPtr(GroupIndex), Element))
			{
				Groups[GroupIndex]->SetObjectField(Source.JsonKey, Element);
			}
		}
	}

	TSharedRef<FJsonObject> AssetSettings = MakeShared<FJsonObject>();
	for (const TCHAR* PropertyName : GAssetPropertyNames)
	{
		FProperty* Property = GroomClass->FindPropertyByName(PropertyName);
		if (!Property)
		{
			NotFound.Add(MakeShared<FJsonValueString>(FString(PropertyName)));
			continue;
		}

		TSharedPtr<FJsonValue> Value = FJsonObjectConverter::UPropertyToJsonValue(
			Property, Property->ContainerPtrToValuePtr<void>(InGroomAsset));
		if (Value.IsValid())
		{
			// Same key casing as the struct fields UStructToJsonObject writes.
			AssetSettings->SetField(FJsonObjectConverter::StandardizeCase(PropertyName), Value);
		}
	}

	TArray<TSharedPtr<FJsonValue>> GroupValues;
	for (const TSharedPtr<FJsonObject>& Group : Groups)
	{
		GroupValues.Add(MakeShared<FJsonValueObject>(Group));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("format"), TEXT("groom_physics_settings"));
	Root->SetNumberField(TEXT("format_version"), 1);
	Root->SetStringField(TEXT("generator"), TEXT("Groom Alembic Exporter (GroomExport plugin)"));
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("groom_asset"), InGroomAsset->GetPathName());
	Root->SetStringField(TEXT("units"),
		TEXT("Values are copied verbatim from the Groom Asset, in Unreal units: centimeters, Z-up, gravity in cm/s^2. ")
		TEXT("They do not follow the coordinate system or unit scale the .abc was written with."));
	Root->SetObjectField(TEXT("export"), MakeExportObject(InSettings, InExportResult));
	Root->SetObjectField(TEXT("asset"), AssetSettings);
	Root->SetArrayField(TEXT("groups"), GroupValues);
	if (NotFound.Num() > 0)
	{
		Root->SetArrayField(TEXT("not_found_in_this_engine_version"), NotFound);
	}

	FString JsonText;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, JsonWriter))
	{
		OutError = LOCTEXT("PhysicsSerializeFailed", "the settings could not be serialized to JSON.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *InFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(
			LOCTEXT("PhysicsSaveFailedFmt", "could not write '{0}'."),
			FText::FromString(InFilename));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
