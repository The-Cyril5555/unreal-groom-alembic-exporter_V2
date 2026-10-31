// Groom Alembic Exporter

#include "SGroomExportOptionsDialog.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GroomExport"

namespace GroomExportOptionsDialog
{
	namespace Private
	{
		/** Slate contents of the modal options window. */
		class SOptionsPanel : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SOptionsPanel) {}
				SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
				SLATE_ARGUMENT(FGroomExportSettings, InitialSettings)
				SLATE_ARGUMENT(int32, NumAssets)
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs)
			{
				ParentWindow = InArgs._ParentWindow;
				Settings = InArgs._InitialSettings;
				NumAssets = InArgs._NumAssets;
				bApplyToAll = NumAssets > 1;
				bConfirmed = false;

				TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);

				// Presets: one click sets every option below for the usual destinations.
				Options->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("Presets", "Presets:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("PresetUnreal", "Unreal round trip"))
						.ToolTipText(LOCTEXT("PresetUnrealTip",
							"The defaults: Unreal space, guides kept with the strands. Re-importing the file into "
							"Unreal with the groom importer's default settings reproduces the original groom."))
						.OnClicked_Lambda([this]()
						{
							Settings = FGroomExportSettings::MakeUnrealRoundTrip();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("PresetBlender", "Blender"))
						.ToolTipText(LOCTEXT("PresetBlenderTip",
							"Y-up, centimeters, guide curves left out, physics settings written next to the file.\n\n"
							"Blender's Alembic importer ignores the groom_guide attribute, so guides kept in the "
							"file are rendered as extra hair."))
						.OnClicked_Lambda([this]()
						{
							Settings = FGroomExportSettings::MakeBlender();
							return FReply::Handled();
						})
					]
				];

				Options->AddSlot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeCheckBox(
						LOCTEXT("YUp", "Convert to Y-up (Maya / Blender)"),
						LOCTEXT("YUpTip",
							"Off (default): positions are written exactly as Unreal stores them (Z-up, centimeters). "
							"This is the round-trip safe option - re-importing the file with the groom importer's "
							"default conversion settings reproduces the original groom.\n\n"
							"On: swaps Y and Z to the Y-up right-handed convention Maya, Blender and Houdini use. "
							"The groom keeps its shape and is not mirrored, but re-importing it into Unreal then "
							"needs the importer's conversion rotation set to (90, 0, 0)."),
						TAttribute<ECheckBoxState>::CreateLambda([this]()
						{
							return Settings.CoordSystem == EGroomExportCoordSystem::YUpRightHanded
								? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}),
						FOnCheckStateChanged::CreateLambda([this](ECheckBoxState NewState)
						{
							Settings.CoordSystem = (NewState == ECheckBoxState::Checked)
								? EGroomExportCoordSystem::YUpRightHanded
								: EGroomExportCoordSystem::UnrealZUp;
						}))
				];

				Options->AddSlot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Scale", "Unit scale"))
						.ToolTipText(LOCTEXT("ScaleTip",
							"Uniform scale applied to positions and widths. Groom assets store centimeters and "
							"Unreal's groom importer reads Alembic units as centimeters, so leave this at 1.0 for a "
							"round trip. Use 0.01 to write the file in meters."))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SSpinBox<float>)
						.MinValue(0.000001f)
						.MaxValue(10000.f)
						.MinSliderValue(0.01f)
						.MaxSliderValue(100.f)
						.Value_Lambda([this]() { return Settings.UnitScale; })
						.OnValueChanged_Lambda([this](float NewValue) { Settings.UnitScale = NewValue; })
					]
				];

				AddSimpleCheckBox(Options,
					LOCTEXT("SplitByGroup", "One curves object per hair group"),
					LOCTEXT("SplitByGroupTip",
						"Writes a separate OCurves object per groom group (group_0, group_1, ...). "
						"Turn off to write every strand into a single OCurves object."),
					Settings.bSplitByGroup);

				AddSimpleCheckBox(Options,
					LOCTEXT("Widths", "Export widths"),
					LOCTEXT("WidthsTip", "Writes the groom's strand widths as the Alembic width parameter, when the groom has them."),
					Settings.bExportWidths);

				AddSimpleCheckBox(Options,
					LOCTEXT("RootUVs", "Export root UVs"),
					LOCTEXT("RootUVsTip", "Writes the per-strand root UVs as the Alembic uv parameter, when the groom has them."),
					Settings.bExportRootUVs);

				AddSimpleCheckBox(Options,
					LOCTEXT("GroomAttrs", "Export groom_* attributes"),
					LOCTEXT("GroomAttrsTip",
						"Writes the groom's remaining attributes (groom_group_id, groom_id, groom_guide, "
						"groom_clumpid, groom_color, ...) as arbitrary geometry parameters. Needed for a faithful "
						"round trip back into Unreal."),
					Settings.bExportGroomAttributes);

				Options->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GuidesHeader", "Guide curves (simulation guides, flagged groom_guide):"))
					.ToolTipText(LOCTEXT("GuidesHeaderTip",
						"Guides drive the hair physics and are not meant to be rendered. Unreal tells them apart "
						"through the groom_guide attribute, but most DCC importers - Blender's included - ignore "
						"it and render them as extra hair."))
				];

				AddGuideModeRadio(Options, EGroomAlembicGuideMode::Mixed,
					LOCTEXT("GuidesMixed", "Keep with the strands (Unreal round trip)"),
					LOCTEXT("GuidesMixedTip",
						"Guides stay in the same curves objects as the strands, flagged by groom_guide. "
						"Re-imports into Unreal exactly, as long as 'Export groom_* attributes' is on (without "
						"it nothing marks them as guides any more). Blender renders them as hair."));

				AddGuideModeRadio(Options, EGroomAlembicGuideMode::Separate,
					LOCTEXT("GuidesSeparate", "Write to separate objects (guides_group_N)"),
					LOCTEXT("GuidesSeparateTip",
						"Guides go into their own curves objects, always flagged with groom_guide = 1, so they can "
						"be hidden, deleted or used for simulation in any DCC. Unreal's importer still reads them "
						"back as guides."));

				AddGuideModeRadio(Options, EGroomAlembicGuideMode::Exclude,
					LOCTEXT("GuidesExclude", "Leave out (render strands only)"),
					LOCTEXT("GuidesExcludeTip",
						"Guides are not written, nor the strands' precomputed guide weights "
						"(groom_closest_guides / groom_guide_weights), which would point at them. Recommended "
						"for Blender rendering. Unreal regenerates guides from the strands if the file is "
						"imported back."));

				AddSimpleCheckBox(Options,
					LOCTEXT("Physics", "Write physics settings (.physics.json)"),
					LOCTEXT("PhysicsTip",
						"Also writes <file>.physics.json next to the .abc with the groom's per-group simulation "
						"settings: stiffness and damping, gravity and air drag, collisions, guide interpolation. "
						"Values are in Unreal units (centimeters, Z-up)."),
					Settings.bWritePhysicsSettings);

				if (NumAssets > 1)
				{
					Options->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						MakeCheckBox(
							FText::Format(LOCTEXT("ApplyAllFmt", "Use these settings for all {0} selected grooms"), FText::AsNumber(NumAssets)),
							LOCTEXT("ApplyAllTip", "Skips this dialog for the remaining grooms in the selection."),
							TAttribute<ECheckBoxState>::CreateLambda([this]()
							{
								return bApplyToAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							}),
							FOnCheckStateChanged::CreateLambda([this](ECheckBoxState NewState)
							{
								bApplyToAll = (NewState == ECheckBoxState::Checked);
							}))
					];
				}

				ChildSlot
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(12.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							Options
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 16.f, 0.f, 0.f)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
							.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
							.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
							+ SUniformGridPanel::Slot(0, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
								.Text(LOCTEXT("ExportButton", "Export"))
								.OnClicked(this, &SOptionsPanel::OnConfirm)
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
								.Text(LOCTEXT("CancelButton", "Cancel"))
								.OnClicked(this, &SOptionsPanel::OnCancel)
							]
						]
					]
				];
			}

			bool WasConfirmed() const { return bConfirmed; }
			bool ShouldApplyToAll() const { return bApplyToAll; }
			const FGroomExportSettings& GetSettings() const { return Settings; }

		private:
			TSharedRef<SWidget> MakeCheckBox(
				const FText& Label,
				const FText& ToolTip,
				TAttribute<ECheckBoxState> IsChecked,
				FOnCheckStateChanged OnChanged)
			{
				return SNew(SCheckBox)
					.IsChecked(IsChecked)
					.OnCheckStateChanged(OnChanged)
					.ToolTipText(ToolTip)
					[
						SNew(STextBlock).Text(Label).ToolTipText(ToolTip)
					];
			}

			/** Adds one radio button of the guide mode choice. */
			void AddGuideModeRadio(TSharedRef<SVerticalBox> Container, EGroomAlembicGuideMode Mode, const FText& Label, const FText& ToolTip)
			{
				Container->AddSlot().AutoHeight().Padding(16.f, 1.f, 0.f, 1.f)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "RadioButton")
					.ToolTipText(ToolTip)
					.IsChecked_Lambda([this, Mode]()
					{
						return Settings.GuideMode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Mode](ECheckBoxState NewState)
					{
						if (NewState == ECheckBoxState::Checked)
						{
							Settings.GuideMode = Mode;
						}
					})
					[
						SNew(STextBlock).Text(Label).ToolTipText(ToolTip)
					]
				];
			}

			/** Adds a checkbox bound directly to a bool member of Settings. */
			void AddSimpleCheckBox(TSharedRef<SVerticalBox> Container, const FText& Label, const FText& ToolTip, bool& Value)
			{
				bool* ValuePtr = &Value;
				Container->AddSlot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeCheckBox(Label, ToolTip,
						TAttribute<ECheckBoxState>::CreateLambda([ValuePtr]()
						{
							return *ValuePtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}),
						FOnCheckStateChanged::CreateLambda([ValuePtr](ECheckBoxState NewState)
						{
							*ValuePtr = (NewState == ECheckBoxState::Checked);
						}))
				];
			}

			FReply OnConfirm()
			{
				bConfirmed = true;
				CloseWindow();
				return FReply::Handled();
			}

			FReply OnCancel()
			{
				bConfirmed = false;
				CloseWindow();
				return FReply::Handled();
			}

			void CloseWindow()
			{
				if (TSharedPtr<SWindow> Window = ParentWindow.Pin())
				{
					Window->RequestDestroyWindow();
				}
			}

			TWeakPtr<SWindow> ParentWindow;
			FGroomExportSettings Settings;
			int32 NumAssets = 1;
			bool bApplyToAll = false;
			bool bConfirmed = false;
		};
	} // namespace Private

	EResult Show(FGroomExportSettings& InOutSettings, bool& bOutApplyToAll, int32 InNumAssets)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("DialogTitle", "Export Groom to Alembic"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		TSharedRef<Private::SOptionsPanel> Panel = SNew(Private::SOptionsPanel)
			.ParentWindow(Window)
			.InitialSettings(InOutSettings)
			.NumAssets(InNumAssets);

		Window->SetContent(Panel);

		if (GEditor)
		{
			GEditor->EditorAddModalWindow(Window);
		}
		else
		{
			FSlateApplication::Get().AddModalWindow(Window, nullptr);
		}

		if (!Panel->WasConfirmed())
		{
			return EResult::Cancel;
		}

		InOutSettings = Panel->GetSettings();
		bOutApplyToAll = Panel->ShouldApplyToAll();
		return EResult::Export;
	}
} // namespace GroomExportOptionsDialog

#undef LOCTEXT_NAMESPACE
