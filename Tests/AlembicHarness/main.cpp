// Drives the plugin's real FGroomAlembicWriter::Export on synthetic grooms.
// Built twice: against the 1.0 sources (HARNESS_V11=0) and the 1.1 sources (HARNESS_V11=1).
// Usage: harness <out_dir>

#include "GroomAlembicWriter.h"
#include "GroomAsset.h"
#include "HairAttributes.h"

#include <cstdio>
#include <string>

namespace
{
	struct FGroomSpec
	{
		const char* Name;
		bool bGuideAttribute = true;   // has a groom_guide strand attribute
		bool bAllGuides = false;       // every strand is a guide
		bool bVertexWidth = true;      // per-vertex widths (else per-strand)
		bool bGuideWeights = false;    // precomputed groom_closest_guides / groom_guide_weights
		bool bOddGuideValues = false;  // some groom_guide values are 2 (not a guide for Unreal)
	};

	bool IsGuide(int32 StrandIndex) { return StrandIndex % 9 == 0 || StrandIndex % 9 == 4; }

	UGroomAsset MakeGroom(const FGroomSpec& Spec)
	{
		UGroomAsset Asset;
		Asset.Name = Spec.Name;
		FHairDescription& D = Asset.Description;

		const int32 NumStrands = 100;
		std::vector<int> VertexCount, GroupID, Guide, StrandID, ClumpID;
		std::vector<FVector2f> RootUV;
		std::vector<float> StrandWidth;
		std::vector<FName> HairName, Basis;
		std::vector<FVector3f> Positions, Colors;
		std::vector<float> VertexWidth;

		for (int32 S = 0; S < NumStrands; ++S)
		{
			const int32 Count = 3 + (S * 7) % 8;
			VertexCount.push_back(Count);
			GroupID.push_back(S < 60 ? 3 : 1);           // non-sequential ids, first seen: 3 then 1
			Guide.push_back(Spec.bAllGuides || IsGuide(S) ? (Spec.bOddGuideValues && S % 9 == 4 ? 2 : 1) : 0);
			StrandID.push_back(1000 + S);
			ClumpID.push_back(S / 10);
			RootUV.push_back(FVector2f(S * 0.01f, 1.f - S * 0.01f));
			StrandWidth.push_back(0.01f + S * 0.0001f);
			HairName.push_back(FName(S % 2 ? "odd" : "even"));
			Basis.push_back(FName("bspline"));
			for (int32 K = 0; K < Count; ++K)
			{
				Positions.push_back(FVector3f(S * 0.1f, K * 0.5f + S * 0.01f, 170.f + K * 0.3f));
				Colors.push_back(FVector3f(0.1f * K, 0.2f, 0.3f));
				VertexWidth.push_back(0.02f - K * 0.001f);
			}
		}

		D.NumStrands = NumStrands;
		D.NumVertices = (int32)Positions.size();

		D.Vertices.Register<FVector3f>(HairAttribute::Vertex::Position, Positions);
		if (Spec.bVertexWidth) { D.Vertices.Register<float>(HairAttribute::Vertex::Width, VertexWidth); }
		D.Vertices.Register<FVector3f>(HairAttribute::Vertex::Color, Colors);

		D.Strands.Register<int>(HairAttribute::Strand::VertexCount, VertexCount);
		D.Strands.Register<int>(HairAttribute::Strand::GroupID, GroupID);
		if (Spec.bGuideAttribute) { D.Strands.Register<int>(HairAttribute::Strand::Guide, Guide); }
		D.Strands.Register<int>(HairAttribute::Strand::ID, StrandID);
		D.Strands.Register<int>(HairAttribute::Strand::ClumpID, ClumpID);
		D.Strands.Register<FVector2f>(HairAttribute::Strand::RootUV, RootUV);
		if (!Spec.bVertexWidth) { D.Strands.Register<float>(HairAttribute::Strand::Width, StrandWidth); }
		D.Strands.Register<FName>(FName("groom_hair_name"), HairName);
		D.Strands.Register<FName>(HairAttribute::Strand::BasisType, Basis);
		if (Spec.bGuideWeights)
		{
			std::vector<FVector3f> Closest, Weights;
			for (int32 S = 0; S < NumStrands; ++S)
			{
				Closest.push_back(FVector3f(1000.f, 1004.f, 1009.f));  // groom_id of guide strands 0, 4, 9
				Weights.push_back(FVector3f(0.5f, 0.3f, 0.2f));
			}
			D.Strands.Register<FVector3f>(FName("groom_closest_guides"), Closest);
			D.Strands.Register<FVector3f>(FName("groom_guide_weights"), Weights);
		}

		D.Groom.Register<int>(HairAttribute::Groom::MajorVersion, { 1 });
		D.Groom.Register<FName>(HairAttribute::Groom::Tool, { FName("Houdini") });
		D.Groom.Register<float>(FName("groom_custom_scale"), { 2.5f });
		return Asset;
	}

	int GFailures = 0;

	void Export(const UGroomAsset& Groom, const std::string& Path, const FGroomExportSettings& Settings, bool bExpectSuccess = true)
	{
		FGroomExportResult Result;
		const bool bOk = FGroomAlembicWriter::Export(&Groom, FString(Path), Settings, Result);
		std::printf("%s %-34s groups=%d curves=%d points=%d", bOk ? "OK  " : "FAIL", Path.c_str(), Result.NumGroups, Result.NumCurves, Result.NumPoints);
#if HARNESS_V11
		std::printf(" guides=%d guide_points=%d skipped=%d objects=[", Result.NumGuideCurves, Result.NumGuidePoints, Result.NumGuideCurvesSkipped);
		for (const FGroomExportCurvesObject& O : Result.CurvesObjects)
		{
			std::printf(" %s(g%d%s c%d p%d guides%d)", *O.Name, O.GroupID, O.bGuidesOnly ? " guides-only" : "", O.NumCurves, O.NumPoints, O.NumGuideCurves);
		}
		std::printf(" ] abc=%s", *Result.AlembicFile);
#else
		std::printf(" skipped=%d", Result.NumGuideCurvesSkipped);
#endif
		if (!bOk) { std::printf(" error=\"%s\"", *Result.ErrorText.ToString()); }
		std::printf("\n");
		if (bOk != bExpectSuccess)
		{
			std::printf("UNEXPECTED %s\n", bOk ? "success" : "failure");
			++GFailures;
		}
	}
}

int main(int Argc, char** Argv)
{
	const std::string Out = Argc > 1 ? Argv[1] : ".";
	const UGroomAsset Groom = MakeGroom({ "Fake" });
	const UGroomAsset NoGuideAttr = MakeGroom({ "NoGuideAttr", false });
	const UGroomAsset AllGuides = MakeGroom({ "AllGuides", true, true });
	const UGroomAsset StrandWidth = MakeGroom({ "StrandWidth", true, false, false });
	const UGroomAsset Weights = MakeGroom({ "Weights", true, false, true, true });
	const UGroomAsset OddGuides = MakeGroom({ "OddGuides", true, false, true, false, true });

#if HARNESS_V11
	const std::string Tag = Out + "/v11_";
	auto Settings = [](EGroomAlembicGuideMode Mode)
	{
		FGroomExportSettings S;
		S.GuideMode = Mode;
		return S;
	};
	const EGroomAlembicGuideMode Mixed = EGroomAlembicGuideMode::Mixed;
	const EGroomAlembicGuideMode Separate = EGroomAlembicGuideMode::Separate;
	const EGroomAlembicGuideMode Exclude = EGroomAlembicGuideMode::Exclude;
#else
	const std::string Tag = Out + "/v10_";
	enum EMode { Mixed, Exclude };
	auto Settings = [](EMode Mode)
	{
		FGroomExportSettings S;
		S.bExportGuideCurves = (Mode == Mixed);
		return S;
	};
#endif

	// Same settings in both versions: these files must be byte-identical between 1.0 and 1.1.
	Export(Groom, Tag + "mixed.abc", Settings(Mixed));
	Export(Groom, Tag + "exclude.abc", Settings(Exclude));
	{ auto S = Settings(Mixed); S.bSplitByGroup = false; Export(Groom, Tag + "mixed_nosplit.abc", S); }
	{ auto S = Settings(Exclude); S.bExportGroomAttributes = false; Export(Groom, Tag + "exclude_noattrs.abc", S); }
	{ auto S = Settings(Mixed); S.CoordSystem = EGroomExportCoordSystem::YUpRightHanded; Export(Groom, Tag + "mixed_yup.abc", S); }
	{ auto S = Settings(Exclude); S.CoordSystem = EGroomExportCoordSystem::YUpRightHanded; Export(Groom, Tag + "exclude_yup.abc", S); }
	{ auto S = Settings(Mixed); S.UnitScale = 0.01f; Export(StrandWidth, Tag + "strandwidth_mixed_m.abc", S); }
	Export(NoGuideAttr, Tag + "noguideattr_mixed.abc", Settings(Mixed));
	Export(AllGuides, Tag + "allguides_exclude.abc", Settings(Exclude), /*bExpectSuccess*/ false);
	Export(Weights, Tag + "weights_mixed.abc", Settings(Mixed));
	Export(OddGuides, Tag + "oddguides_mixed.abc", Settings(Mixed));

	// 1.1 changed these on purpose: leave-out drops the guide weights, and only groom_guide == 1
	// counts as a guide, as in Unreal's groom builder.
	Export(Weights, Tag + "weights_exclude.abc", Settings(Exclude));
	Export(OddGuides, Tag + "oddguides_exclude.abc", Settings(Exclude));

#if HARNESS_V11
	// 1.1 only.
	Export(Groom, Tag + "separate.abc", Settings(Separate));
	{ auto S = Settings(Separate); S.bExportGroomAttributes = false; Export(Groom, Tag + "separate_noattrs.abc", S); }
	{ auto S = Settings(Separate); S.bSplitByGroup = false; Export(Groom, Tag + "separate_nosplit.abc", S); }
	{ auto S = Settings(Separate); S.CoordSystem = EGroomExportCoordSystem::YUpRightHanded; Export(Groom, Tag + "separate_yup.abc", S); }
	{ auto S = FGroomExportSettings::MakeBlender(); S.bWritePhysicsSettings = false; Export(Groom, Tag + "blender_preset.abc", S); }
	Export(NoGuideAttr, Tag + "noguideattr_separate.abc", Settings(Separate));
	Export(AllGuides, Tag + "allguides_separate.abc", Settings(Separate));
	{ auto S = Settings(Separate); S.bWritePhysicsSettings = true; Export(Groom, Tag + "separate_physics_stub.abc", S); }
	Export(Weights, Tag + "weights_separate.abc", Settings(Separate));
	Export(OddGuides, Tag + "oddguides_separate.abc", Settings(Separate));
#endif

	std::printf("---- %d unexpected result(s)\n", GFailures);
	return GFailures ? 1 : 0;
}
