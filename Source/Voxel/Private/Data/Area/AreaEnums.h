// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/Enum/GameStateEnum.h"
#include "Data/CoreFragment.h"
#include "Engine/DataTable.h"
#include "AreaEnums.generated.h"


enum class EVoxelBuildLimit : uint8;

// UENUM(BlueprintType)
// enum class EAreaType : uint8
// {
// 	SpawnArea				=0		UMETA(DisplayName="出生区域"),
// 	
// 	
// 	
// 	ConnectArea				=3		UMETA(DisplayName="链接区域"),
// 	DefaultArea				=4		UMETA(DisplayName="通用区域"),
//
// 	BusinessClusterArea     =1		UMETA(DisplayName="商圈区域"),
// 	
// 	ShopArea				=2		UMETA(DisplayName="店铺区域"),
// 	SubFunctionArea			=6		UMETA(DisplayName="子功能区域"),
// 	PublicFunctionArea		=7		UMETA(DisplayName="公共职能区域"),
// };

// 建议添加区域状态枚举
UENUM(BlueprintType)
enum class EAreaStatus : uint8
{
	Planning			UMETA(DisplayName="规划中"),
	UnderConstruction	UMETA(DisplayName="建设中"),
	Active				UMETA(DisplayName="运营中"),
	Suspended			UMETA(DisplayName="暂停营业"),
	Closed				UMETA(DisplayName="已关闭")
};




USTRUCT(BlueprintType)
struct FAreaTypeBase : public FTableRowBase
{
	GENERATED_USTRUCT_BODY();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	int AreaTypeId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	FGameplayTag AreaTypeTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	FText DisplayName; // 显示名称（支持本地化）
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	FText Description; // 区域描述

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	bool bAbstract = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	FVoxelVisualFragment AssetFragment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	TArray<int> RequiredFurnitureType; // 必需家具列表

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	TArray<int> ForbiddenFurnitureType; // 限制家具列表
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	TArray<FGameplayTag> RequiredParentTypes; // 必需父区域类型
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	TArray<FGameplayTag> AllowedParentTypes; // 允许的父区域
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	EVoxelBuildLimit BuildLimit; // 是否可由玩家建造

	
	// 辅助函数
	bool IsShopType() const
	{
		// 静态变量缓存 Tag，优化性能
		static const FGameplayTag ShopTag = FGameplayTag::RequestGameplayTag(FName("Area.Type.Shop"));
		// MatchesTag: 如果 AreaTypeId 是 "Area.Type.Shop.Bakery"，它会匹配 "Area.Type.Shop"
		return AreaTypeTag.MatchesTag(ShopTag); 
	}
    
	// 判断是否为子功能区
	bool IsSubFunctionType() const
	{
		static const FGameplayTag SubFuncTag = FGameplayTag::RequestGameplayTag(FName("Area.Type.Shop.SubFunction"));
		return AreaTypeTag.MatchesTag(SubFuncTag);
	}
    
	// 检查是否依赖特定父级
	bool NeedsSpecificParent() const
	{
		return AllowedParentTypes.Num() > 0 || RequiredParentTypes.Num() > 0;
	}
	
	// 检查父区域类型是否合法 (参数改为 Tag)
	bool CheckAllowParentType(FGameplayTag ParentTypeTag) const
	{
		if (NeedsSpecificParent())
		{
			// 1. 检查是否在允许列表中
			for (const FGameplayTag& AllowedTag : AllowedParentTypes)
			{
				// MatchesTag: 父区域是 "Area.Type.Council.Block"，允许 "Area.Type.Council"，则匹配成功
				if (ParentTypeTag.MatchesTag(AllowedTag)) 
				{
					return true;
				}
			}
			// 2. (可选) 这里也可以添加 RequiredParentTypes 的检查逻辑，视业务需求而定
			
			return false; // 需要特定父级但未匹配到
		}
		
		return true; // 不需要特定父级，则允许
	}

	bool CanBeBuild(EVoxelBuildLimit input) const
	{
		if (input == EVoxelBuildLimit::Develop)
		{
			return true;
		}
		if (input == BuildLimit)
		{
			return true;
		}
		return false;
	}
};

