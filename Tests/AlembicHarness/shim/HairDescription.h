// Stand-in for HairStrandsCore's FHairDescription: typed attribute arrays per scope.
#pragma once

#include "CoreMinimal.h"

#include <memory>

struct FStrandID { int32 Idx; explicit FStrandID(int32 I) : Idx(I) {} };
struct FVertexID { int32 Idx; explicit FVertexID(int32 I) : Idx(I) {} };
struct FGroomID  { int32 Idx; explicit FGroomID(int32 I) : Idx(I) {} };

template <typename IDType, typename T>
class TAttributesRefShim
{
public:
	std::vector<T>* Data = nullptr;
	TAttributesRefShim() {}
	explicit TAttributesRefShim(std::vector<T>* InData) : Data(InData) {}
	bool IsValid() const { return Data != nullptr; }
	T& operator[](IDType ID) const { return Data->at(ID.Idx); }
};

template <typename T> using TStrandAttributesRef = TAttributesRefShim<FStrandID, T>;
template <typename T> using TVertexAttributesRef = TAttributesRefShim<FVertexID, T>;
template <typename T> using TGroomAttributesRef  = TAttributesRefShim<FGroomID, T>;

/** One scope's attributes. Names keep registration order, like UE's attribute map. */
template <typename IDType>
class TAttributeSetShim
{
public:
	std::vector<std::string> Order;
	std::unordered_map<std::string, std::vector<int>>       Ints;
	std::unordered_map<std::string, std::vector<float>>     Floats;
	std::unordered_map<std::string, std::vector<FVector2f>> Vec2s;
	std::unordered_map<std::string, std::vector<FVector3f>> Vec3s;
	std::unordered_map<std::string, std::vector<FName>>     Names;

	template <typename T> std::unordered_map<std::string, std::vector<T>>& Store()
	{
		if constexpr (std::is_same_v<T, int>) return Ints;
		else if constexpr (std::is_same_v<T, float>) return Floats;
		else if constexpr (std::is_same_v<T, FVector2f>) return Vec2s;
		else if constexpr (std::is_same_v<T, FVector3f>) return Vec3s;
		else return Names;
	}

	template <typename T> void Register(const FName& Name, std::vector<T> Values)
	{
		Store<T>()[Name.S] = std::move(Values);
		if (std::find(Order.begin(), Order.end(), Name.S) == Order.end())
		{
			Order.push_back(Name.S);
		}
	}

	template <typename T> TAttributesRefShim<IDType, T> GetAttributesRef(const FName& Name)
	{
		auto& Map = Store<T>();
		auto It = Map.find(Name.S);
		return It == Map.end() ? TAttributesRefShim<IDType, T>() : TAttributesRefShim<IDType, T>(&It->second);
	}

	void GetAttributeNames(TArray<FName>& Out) const
	{
		for (const std::string& N : Order) { Out.Add(FName(N.c_str())); }
	}
};

class FHairDescription
{
public:
	TAttributeSetShim<FStrandID> Strands;
	TAttributeSetShim<FVertexID> Vertices;
	TAttributeSetShim<FGroomID>  Groom;
	int32 NumStrands = 0;
	int32 NumVertices = 0;

	int32 GetNumStrands() const { return NumStrands; }
	int32 GetNumVertices() const { return NumVertices; }
	TAttributeSetShim<FStrandID>& StrandAttributes() { return Strands; }
	TAttributeSetShim<FVertexID>& VertexAttributes() { return Vertices; }
	TAttributeSetShim<FGroomID>&  GroomAttributes()  { return Groom; }
};
