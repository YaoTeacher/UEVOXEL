// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "VoxelTimeDataBase.generated.h"

UENUM(Blueprintable,BlueprintType)
enum class WeekEnums : uint8
{
	Monday = 0,
	Tuesday = 1,
	Wednesday = 2,
	Thursday = 3,
	Friday = 4,
	Saturday = 5,
	Sunday = 6,
	
};

USTRUCT(BlueprintType)
struct FBusinessHours : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Time")
	int StartHours;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Time")
	int EndHours;

	
};

USTRUCT(BlueprintType)
struct FBusinessSchedule : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Time")
	TArray<WeekEnums> WeekDays;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Time")
	TArray<FBusinessHours> Hours;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Time")
	TArray<int> ResDates;


	
};