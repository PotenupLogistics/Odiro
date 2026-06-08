#pragma once

#include "CoreMinimal.h"
#include "DeliveryBotPolicyCatalogSourceInfo.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyCatalogSourceEntryInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CatalogId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CatalogVersion{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RelativePath{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PolicyCount{ 0 };
};

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyCatalogSourcesInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWasSuccessful{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResponseCode{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RawResponseBody{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Status{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActiveCatalogId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotPolicyCatalogSourceEntryInfo> Sources{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorMessage{};
};

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyCatalogPolicyInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PolicyId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Category{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDefaultEnabled{ true };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DefaultPriority{ 100 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresGrid{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresGoal{ false };
};

USTRUCT(BlueprintType)
struct FDeliveryBotPolicyCatalogInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWasSuccessful{ false };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ResponseCode{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RawResponseBody{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Status{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ActiveCatalogId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CatalogId{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CatalogVersion{ 0 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDeliveryBotPolicyCatalogPolicyInfo> Policies{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ErrorMessage{};
};