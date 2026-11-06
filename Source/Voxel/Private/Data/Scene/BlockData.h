// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BlockData.generated.h"

USTRUCT(BlueprintType)
struct FBlockUpdate
{
	GENERATED_BODY()
    
	UPROPERTY(BlueprintReadWrite, Category = "Block")
	FIntVector WorldPos;
    
	UPROPERTY(BlueprintReadWrite, Category = "Block")
	FIntVector LocalPos;
    
	UPROPERTY(BlueprintReadWrite, Category = "Block")
	int OldType = 0;
    
	UPROPERTY(BlueprintReadWrite, Category = "Block")
	int NewType = 0;
    
	double Timestamp = 0.0;
};
/**
 * 
 */
USTRUCT(BlueprintType)
struct FBlockData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Block")
	int BlockId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Block")
	int BlockType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Block")
	int BlockDamageState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Block")
	int ParentScenceId;
};
