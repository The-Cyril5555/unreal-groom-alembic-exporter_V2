// Stand-in for HairAttributes.h: the attribute names are FName constants.
#pragma once
#include "CoreMinimal.h"
namespace HairAttribute
{
	namespace Vertex
	{
		inline const FName Position("position");
		inline const FName Width("groom_width");
		inline const FName Color("groom_color");
	}
	namespace Strand
	{
		inline const FName VertexCount("vertexcount");
		inline const FName Width("groom_width");
		inline const FName RootUV("groom_root_uv");
		inline const FName Guide("groom_guide");
		inline const FName GroupID("groom_group_id");
		inline const FName ID("groom_id");
		inline const FName ClumpID("groom_clumpid");
		inline const FName BasisType("groom_basis_type");
		inline const FName CurveType("groom_type");
		inline const FName Knots("groom_knots");
	}
	namespace Groom
	{
		inline const FName MajorVersion("groom_version_major");
		inline const FName MinorVersion("groom_version_minor");
		inline const FName Tool("groom_tool");
	}
}
