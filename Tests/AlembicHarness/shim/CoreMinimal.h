// Minimal stand-in for the parts of Unreal's Core that GroomAlembicWriter.cpp uses, so the
// writer can be compiled verbatim against the real Alembic library on Linux. TCHAR is char.
#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

typedef int32_t int32;
typedef uint8_t uint8;
typedef uint32_t uint32;
typedef int64_t int64;
typedef char TCHAR;
typedef char ANSICHAR;

#define TEXT(x) x
#define FORCEINLINE inline
#define PLATFORM_WINDOWS 0
#define WITH_EDITORONLY_DATA 1
#define THIRD_PARTY_INCLUDES_START
#define THIRD_PARTY_INCLUDES_END
#define TCHAR_TO_UTF8(x) (x)
#define ANSI_TO_TCHAR(x) (x)
#define UENUM(...)
#define USTRUCT(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define UMETA(...)
#define GENERATED_BODY()
#define GROOMEXPORTEDITOR_API

template <typename T>
typename std::remove_reference<T>::type&& MoveTemp(T&& Value) { return std::move(Value); }

class FString
{
public:
	std::string S;
	FString() {}
	FString(const char* C) : S(C ? C : "") {}
	FString(const std::string& In) : S(In) {}
	const char* operator*() const { return S.c_str(); }
	bool IsEmpty() const { return S.empty(); }
	bool StartsWith(const char* Prefix) const { return S.rfind(Prefix, 0) == 0; }
	int32 Len() const { return (int32)S.size(); }
	void Reset() { S.clear(); }
	FString operator+(const FString& O) const { return FString(S + O.S); }
	bool operator==(const FString& O) const { return S == O.S; }
	static FString Printf(const char* Fmt, ...)
	{
		char Buf[2048];
		va_list Args;
		va_start(Args, Fmt);
		vsnprintf(Buf, sizeof(Buf), Fmt, Args);
		va_end(Args);
		return FString(Buf);
	}
};

class FName
{
public:
	std::string S;
	FName() {}
	FName(const char* C) : S(C) {}
	FString ToString() const { return FString(S); }
	bool operator==(const FName& O) const { return S == O.S; }
	bool operator!=(const FName& O) const { return S != O.S; }
};

namespace std
{
	template <> struct hash<FName> { size_t operator()(const FName& N) const { return hash<string>()(N.S); } };
}

class FText
{
public:
	std::string S;
	static FText FromString(const FString& In) { FText T; T.S = In.S; return T; }
	static FText AsNumber(int64 N) { FText T; T.S = std::to_string(N); return T; }
	FString ToString() const { return FString(S); }
	bool IsEmpty() const { return S.empty(); }
	template <typename... ArgTypes>
	static FText Format(const FText& Fmt, const ArgTypes&... Args)
	{
		std::vector<std::string> Values{ Args.S... };
		std::string Result = Fmt.S;
		for (size_t I = 0; I < Values.size(); ++I)
		{
			const std::string Key = "{" + std::to_string(I) + "}";
			for (size_t Pos; (Pos = Result.find(Key)) != std::string::npos;)
			{
				Result.replace(Pos, Key.size(), Values[I]);
			}
		}
		FText T;
		T.S = Result;
		return T;
	}
};
#define LOCTEXT(Key, Text) FText::FromString(FString(Text))

template <typename T>
class TArray
{
public:
	std::vector<T> V;
	TArray() {}
	TArray(std::initializer_list<T> L) : V(L) {}
	int32 Num() const { return (int32)V.size(); }
	int32 Add(const T& X) { V.push_back(X); return Num() - 1; }
	int32 Add(T&& X) { V.push_back(std::move(X)); return Num() - 1; }
	int32 AddDefaulted() { V.emplace_back(); return Num() - 1; }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	void SetNumUninitialized(int32 N) { V.resize(N); }
	void Reserve(int32 N) { V.reserve(N); }
	void Reset() { V.clear(); }
	T& operator[](int32 I) { return V.at(I); }
	const T& operator[](int32 I) const { return V.at(I); }
	typename std::vector<T>::iterator begin() { return V.begin(); }
	typename std::vector<T>::iterator end() { return V.end(); }
	typename std::vector<T>::const_iterator begin() const { return V.begin(); }
	typename std::vector<T>::const_iterator end() const { return V.end(); }
	void Append(TArray&& O) { for (T& X : O.V) { V.push_back(std::move(X)); } O.V.clear(); }
	void Append(const TArray& O) { V.insert(V.end(), O.V.begin(), O.V.end()); }
	template <typename P> bool ContainsByPredicate(P Pred) const { return std::any_of(V.begin(), V.end(), Pred); }
};

template <typename K, typename V>
class TMap
{
public:
	std::unordered_map<K, V> M;
	V* Find(const K& Key) { auto It = M.find(Key); return It == M.end() ? nullptr : &It->second; }
	V& Add(const K& Key, const V& Value) { return M[Key] = Value; }
	int32 Num() const { return (int32)M.size(); }
};

template <typename T>
class TSet
{
public:
	std::unordered_set<T> S;
	void Add(const T& X) { S.insert(X); }
	int32 Num() const { return (int32)S.size(); }
};

template <typename F> using TFunctionRef = std::function<F>;

struct FVector3f { float X = 0, Y = 0, Z = 0; FVector3f() {} FVector3f(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {} };
struct FVector2f { float X = 0, Y = 0; FVector2f() {} FVector2f(float InX, float InY) : X(InX), Y(InY) {} };

#define DEFINE_LOG_CATEGORY_STATIC(Name, A, B) [[maybe_unused]] static int Name##_Category = 0;
#define UE_LOG(Category, Verbosity, Fmt, ...) do { std::fprintf(stderr, "[" #Category "] " Fmt "\n", ##__VA_ARGS__); } while (0)
