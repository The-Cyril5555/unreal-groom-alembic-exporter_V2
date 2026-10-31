// Groom Alembic Exporter

#include "GroomExportEditor.h"

#include "GroomAlembicWriter.h"
#include "GroomExportSettings.h"
#include "SGroomExportOptionsDialog.h"

#include "ContentBrowserMenuContexts.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GroomAsset.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogGroomExportEditor, Log, All);

#define LOCTEXT_NAMESPACE "GroomExport"

namespace GroomExportMenu
{
	/** Remembers the last settings the user picked, for the lifetime of the editor session. */
	static FGroomExportSettings GLastUsedSettings;

	/** The last directory an export was written to, used to seed the file dialogs. */
	static FString GLastUsedDirectory;

	static TArray<UGroomAsset*> GetSelectedGrooms(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			return CBContext->LoadSelectedObjects<UGroomAsset>();
		}
		return {};
	}

	static bool CanExport(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		// Deliberately does not load the assets: any selected groom is a plausible target,
		// and whether its source description survived is reported when the export runs.
		return CBContext && CBContext->SelectedAssets.Num() > 0;
	}

	static FString GetDefaultDirectory()
	{
		return GLastUsedDirectory.IsEmpty() ? FPaths::ProjectSavedDir() : GLastUsedDirectory;
	}

	/** Guide curve totals over a batch, to tell the user where the guides went. */
	struct FGuideTotals
	{
		int32 Mixed = 0;
		int32 Separate = 0;
		int32 LeftOut = 0;
		int32 PhysicsFiles = 0;

		void Add(const FGroomExportSettings& Settings, const FGroomExportResult& Result)
		{
			if (Settings.GuideMode == EGroomAlembicGuideMode::Separate)
			{
				Separate += Result.NumGuideCurves;
			}
			else
			{
				Mixed += Result.NumGuideCurves;
			}
			LeftOut += Result.NumGuideCurvesSkipped;
			PhysicsFiles += Result.PhysicsSettingsFile.IsEmpty() ? 0 : 1;
		}

		/** Extra notification lines, empty when there is nothing to say. */
		FText Describe() const
		{
			TArray<FText> Lines;
			if (Mixed > 0)
			{
				Lines.Add(FText::Format(LOCTEXT("GuidesMixedFmt",
					"{0} guide curve(s) kept with the strands. Blender renders them as extra hair: use the "
					"Blender preset, or write guides to separate objects."), FText::AsNumber(Mixed)));
			}
			if (Separate > 0)
			{
				Lines.Add(FText::Format(LOCTEXT("GuidesSeparateFmt",
					"{0} guide curve(s) written to separate guides_* objects."), FText::AsNumber(Separate)));
			}
			if (LeftOut > 0)
			{
				Lines.Add(FText::Format(LOCTEXT("GuidesLeftOutFmt",
					"{0} guide curve(s) left out."), FText::AsNumber(LeftOut)));
			}
			if (PhysicsFiles > 0)
			{
				Lines.Add(LOCTEXT("PhysicsWritten", "Physics settings written next to the .abc (.physics.json)."));
			}
			return FText::Join(FText::FromString(TEXT("\n")), Lines);
		}
	};

	static void NotifyResult(bool bSuccess, const FText& Message)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = bSuccess ? 6.f : 12.f;
		Info.bUseLargeFont = false;
		Info.bFireAndForget = true;

		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	static void ExecuteExport(const FToolMenuContext& InContext)
	{
		TArray<UGroomAsset*> Grooms = GetSelectedGrooms(InContext);
		Grooms.RemoveAll([](const UGroomAsset* Groom) { return Groom == nullptr; });

		if (Grooms.Num() == 0)
		{
			NotifyResult(false, LOCTEXT("NoGroomsSelected", "No groom assets in the selection."));
			return;
		}

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (!DesktopPlatform)
		{
			NotifyResult(false, LOCTEXT("NoDesktopPlatform", "The desktop platform module is unavailable, so no file dialog can be shown."));
			return;
		}

		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		// For a multi-selection, pick the destination folder once and derive file names
		// from the asset names; for a single groom, ask for the full file name.
		FString TargetDirectory;
		if (Grooms.Num() > 1)
		{
			if (!DesktopPlatform->OpenDirectoryDialog(
					ParentWindowHandle,
					LOCTEXT("ChooseFolderTitle", "Choose a folder for the exported .abc files").ToString(),
					GetDefaultDirectory(),
					TargetDirectory))
			{
				return;
			}
			GLastUsedDirectory = TargetDirectory;
		}

		// Collect every settings/destination choice up front: modal dialogs must not be
		// raised while the slow-task progress dialog is up.
		struct FPendingExport
		{
			UGroomAsset* Groom = nullptr;
			FString Filename;
			FGroomExportSettings Settings;
		};

		TArray<FPendingExport> PendingExports;
		PendingExports.Reserve(Grooms.Num());

		FGroomExportSettings SharedSettings = GLastUsedSettings;
		bool bHaveSharedSettings = false;

		for (UGroomAsset* Groom : Grooms)
		{
			// Settings: asked once per groom, unless the user opted to reuse them for the batch.
			FGroomExportSettings Settings = bHaveSharedSettings ? SharedSettings : GLastUsedSettings;
			if (!bHaveSharedSettings)
			{
				bool bApplyToAll = false;
				if (GroomExportOptionsDialog::Show(Settings, bApplyToAll, Grooms.Num()) != GroomExportOptionsDialog::EResult::Export)
				{
					return;
				}
				GLastUsedSettings = Settings;
				if (bApplyToAll)
				{
					SharedSettings = Settings;
					bHaveSharedSettings = true;
				}
			}

			// Destination file.
			FString Filename;
			if (Grooms.Num() > 1)
			{
				Filename = FPaths::Combine(TargetDirectory, Groom->GetName() + TEXT(".abc"));
			}
			else
			{
				TArray<FString> OutFilenames;
				if (!DesktopPlatform->SaveFileDialog(
						ParentWindowHandle,
						LOCTEXT("SaveAbcTitle", "Export Groom to Alembic").ToString(),
						GetDefaultDirectory(),
						Groom->GetName() + TEXT(".abc"),
						TEXT("Alembic file (*.abc)|*.abc"),
						EFileDialogFlags::None,
						OutFilenames)
					|| OutFilenames.Num() == 0)
				{
					return;
				}

				Filename = OutFilenames[0];
				if (FPaths::GetExtension(Filename).IsEmpty())
				{
					Filename += TEXT(".abc");
				}
				GLastUsedDirectory = FPaths::GetPath(Filename);
			}

			PendingExports.Add(FPendingExport{ Groom, MoveTemp(Filename), Settings });
		}

		int32 NumExported = 0;
		int32 NumFailed = 0;
		FText FirstError;
		FString LastWrittenFile;
		FGuideTotals GuideTotals;

		FScopedSlowTask SlowTask(static_cast<float>(PendingExports.Num()), LOCTEXT("ExportingGrooms", "Exporting grooms to Alembic..."));
		SlowTask.MakeDialog(/*bShowCancelButton*/ true);

		for (const FPendingExport& Pending : PendingExports)
		{
			if (SlowTask.ShouldCancel())
			{
				break;
			}
			SlowTask.EnterProgressFrame(1.f, FText::Format(
				LOCTEXT("ExportingGroomFmt", "Exporting {0}..."), FText::FromString(Pending.Groom->GetName())));

			FGroomExportResult Result;
			if (FGroomAlembicWriter::Export(Pending.Groom, Pending.Filename, Pending.Settings, Result))
			{
				++NumExported;
				LastWrittenFile = Pending.Filename;
				GuideTotals.Add(Pending.Settings, Result);
			}
			else
			{
				++NumFailed;
				if (FirstError.IsEmpty())
				{
					FirstError = Result.ErrorText;
				}
				UE_LOG(LogGroomExportEditor, Error, TEXT("Failed to export groom '%s': %s"),
					*Pending.Groom->GetName(), *Result.ErrorText.ToString());
			}
		}

		if (NumFailed > 0)
		{
			NotifyResult(false, FText::Format(
				LOCTEXT("ExportFailedFmt", "Groom export failed for {0} of {1} asset(s).\n{2}"),
				FText::AsNumber(NumFailed), FText::AsNumber(NumExported + NumFailed), FirstError));
		}
		else if (NumExported > 0)
		{
			const FText Summary = (NumExported == 1)
				? FText::Format(LOCTEXT("ExportedOneFmt", "Exported groom to {0}"), FText::FromString(LastWrittenFile))
				: FText::Format(LOCTEXT("ExportedManyFmt", "Exported {0} grooms to {1}"),
					FText::AsNumber(NumExported), FText::FromString(TargetDirectory));

			const FText Details = GuideTotals.Describe();
			NotifyResult(true, Details.IsEmpty()
				? Summary
				: FText::Format(LOCTEXT("ExportedWithDetailsFmt", "{0}\n{1}"), Summary, Details));
		}
	}

	// FToolMenuOwner takes a non-const void*, so the owner cannot be const here.
	static void RegisterMenus(void* Owner)
	{
		FToolMenuOwnerScoped OwnerScoped(Owner);

		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UGroomAsset::StaticClass());
		if (!Menu)
		{
			return;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("GroomExport_Alembic", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = LOCTEXT("ExportGroomToAlembic", "Export Groom to Alembic (.abc)...");
			const TAttribute<FText> ToolTip = LOCTEXT("ExportGroomToAlembicTooltip",
				"Write this groom's strand curves to an Alembic file using Unreal's groom Alembic schema.");
			const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions");

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteExport);
			UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExport);
			InSection.AddMenuEntry("GroomAsset_ExportToAlembic", Label, ToolTip, Icon, UIAction);
		}));
	}
} // namespace GroomExportMenu

void FGroomExportEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		GroomExportMenu::RegisterMenus(this);
	}));
}

void FGroomExportEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGroomExportEditorModule, GroomExportEditor)
