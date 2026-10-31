// Prints what an archive holds: per curves object, counts, channels and arbGeomParams.
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <cstdio>
#include <string>
using namespace Alembic::AbcGeom;

static const char* ScopeName(GeometryScope S)
{
	switch (S) { case kUniformScope: return "uniform"; case kVertexScope: return "vertex"; case kConstantScope: return "constant"; case kVaryingScope: return "varying"; case kFacevaryingScope: return "facevarying"; default: return "unknown"; }
}

static void Walk(IObject Obj)
{
	if (ICurves::matches(Obj.getMetaData()))
	{
		ICurves Curves(Obj, kWrapExisting);
		ICurvesSchema& Schema = Curves.getSchema();
		ICurvesSchema::Sample Sample;
		Schema.get(Sample);
		std::printf("curves %s curves=%zu points=%zu type=%d wrap=%d", Obj.getFullName().c_str(),
			Sample.getCurvesNumVertices()->size(), Sample.getPositions()->size(), (int)Sample.getType(), (int)Sample.getWrap());
		{
			// FNV-1a over positions + vertex counts + widths, to compare objects across files.
			unsigned long long Hash = 1469598103934665603ULL;
			auto Mix = [&Hash](const void* Data, size_t Size) { const unsigned char* B = (const unsigned char*)Data; for (size_t I = 0; I < Size; ++I) { Hash ^= B[I]; Hash *= 1099511628211ULL; } };
			Mix(Sample.getPositions()->getData(), Sample.getPositions()->size() * sizeof(V3f));
			Mix(Sample.getCurvesNumVertices()->getData(), Sample.getCurvesNumVertices()->size() * sizeof(int32_t));
			if (Schema.getWidthsParam().valid()) { FloatArraySamplePtr W = Schema.getWidthsParam().getExpandedValue().getVals(); Mix(W->getData(), W->size() * sizeof(float)); }
			std::printf(" hash=%016llx", Hash);
		}
		if (Schema.getWidthsParam().valid()) { std::printf(" width=%s", ScopeName(Schema.getWidthsParam().getScope())); }
		if (Schema.getUVsParam().valid()) { std::printf(" uv=%s", ScopeName(Schema.getUVsParam().getScope())); }
		std::printf("\n");
		ICompoundProperty Arb = Schema.getArbGeomParams();
		if (Arb.valid())
		{
			for (size_t I = 0; I < Arb.getNumProperties(); ++I)
			{
				const PropertyHeader& H = Arb.getPropertyHeader(I);
				std::printf("   arb %-20s %-8s %s", H.getName().c_str(), PODName(H.getDataType().getPod()), ScopeName(GetGeometryScope(H.getMetaData())));
				if (H.getName() == "groom_guide" && IInt32GeomParam::matches(H))
				{
					IInt32GeomParam P(Arb, H.getName());
					Int32ArraySamplePtr V = P.getExpandedValue().getVals();
					long Sum = 0; for (size_t K = 0; K < V->size(); ++K) Sum += (*V)[K];
					std::printf(" values=%zu sum=%ld", V->size(), Sum);
				}
				std::printf("\n");
			}
		}
	}
	else if (IXform::matches(Obj.getMetaData()))
	{
		IXform X(Obj, kWrapExisting);
		ICompoundProperty U = X.getSchema().getUserProperties();
		std::printf("xform %s user_props=", Obj.getFullName().c_str());
		if (U.valid()) { for (size_t I = 0; I < U.getNumProperties(); ++I) std::printf("%s%s", I ? "," : "", U.getPropertyHeader(I).getName().c_str()); }
		std::printf("\n");
	}
	for (size_t I = 0; I < Obj.getNumChildren(); ++I) { Walk(Obj.getChild(I)); }
}

int main(int Argc, char** Argv)
{
	for (int A = 1; A < Argc; ++A)
	{
		Alembic::AbcCoreFactory::IFactory Factory;
		IArchive Archive = Factory.getArchive(Argv[A]);
		std::printf("==== %s\n", Argv[A]);
		Walk(Archive.getTop());
	}
	return 0;
}
