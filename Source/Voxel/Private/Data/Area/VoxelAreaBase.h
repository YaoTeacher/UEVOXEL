#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AreaEnums.h"
#include "GameplayTagContainer.h"
#include "VoxelAreaBase.generated.h"

USTRUCT(BlueprintType)
struct FAreaCoverageFragment
{
	GENERATED_BODY();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Coverage")
	FIntVector MinIndex= FIntVector::NoneValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Coverage")
	FIntVector MaxIndex= FIntVector::NoneValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Coverage")
	FIntVector CoreOffsetIndex= FIntVector::NoneValue;
	FAreaCoverageFragment()
	{
		MinIndex = FIntVector(0);
		MaxIndex = FIntVector(0);
		CoreOffsetIndex = FIntVector(0);
	}
	
	/**
	 * 获取包围盒（索引空间）
	 * 【修复】MaxIndex 需要加 1 才能包含最后一个体素的完整范围
	 *
	 * 索引空间的语义：
	 * - 索引 (0,0,0) 到 (10,10,10) 表示 11×11×11 个体素
	 * - 每个体素 i 占据的空间是 [i, i+1]
	 * - 所以包围盒应该是 [Min, Max+1]
	 */
	FBox BoundingBox() const
	{
		return FBox(
			FVector(MinIndex + CoreOffsetIndex),
			FVector(MaxIndex + CoreOffsetIndex + FIntVector(1))  // MaxIndex 为闭区间，包围盒需 +1 以包含完整体素
		);
	}
	FVector GetCenter() const
	{
		return FVector((MinIndex + MaxIndex) * 0.5f);
	}
	void SetBoundingBox(const FBox& NewBox)
	{
		// 保证 MinIndex <= MaxIndex（所有调用的统一数据校验点）
		const FIntVector RawMin = FIntVector(NewBox.Min);
		const FIntVector RawMax = FIntVector(NewBox.Max) - FIntVector(1);
		MinIndex = FIntVector(FMath::Min(RawMin.X, RawMax.X), FMath::Min(RawMin.Y, RawMax.Y), FMath::Min(RawMin.Z, RawMax.Z));
		MaxIndex = FIntVector(FMath::Max(RawMin.X, RawMax.X), FMath::Max(RawMin.Y, RawMax.Y), FMath::Max(RawMin.Z, RawMax.Z));
		CoreOffsetIndex = FIntVector(0);
	};
	
	void SetBoundingBox(const FIntVector& Min,const FIntVector& Max)
	{
		MinIndex = FIntVector(FMath::Min(Min.X, Max.X), FMath::Min(Min.Y, Max.Y), FMath::Min(Min.Z, Max.Z));
		MaxIndex = FIntVector(FMath::Max(Min.X, Max.X), FMath::Max(Min.Y, Max.Y), FMath::Max(Min.Z, Max.Z));
		CoreOffsetIndex = FIntVector(0);
	};
	void UpdateOffset(const FIntVector Offset)
	{
		MinIndex += CoreOffsetIndex;
		MaxIndex += CoreOffsetIndex;
		CoreOffsetIndex = FIntVector(0);
		CoreOffsetIndex += Offset;
	};

	bool isValid() const
	{
		return MinIndex != FIntVector::NoneValue&&MaxIndex!= FIntVector::NoneValue;
	}
	
	bool isSimilar() const
	{
		return MinIndex == MaxIndex;
	}

	bool operator==(const FAreaCoverageFragment& Other) const
	{
		return MinIndex == Other.MinIndex && MaxIndex == Other.MaxIndex && CoreOffsetIndex == Other.CoreOffsetIndex;
	}
};

USTRUCT(BlueprintType)
struct FAreaAuthorityBFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/DefaultAuthoritySetting")
	bool bIsPublic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/DefaultAuthoritySetting")
	bool bIsAllowBattle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/DefaultAuthoritySetting")
	TMap<int, EAcceptabilityEnum> AllowClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Area/DefaultAuthoritySetting")
	TMap<int, EAcceptabilityEnum> AllowProf;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Area/DefaultAuthoritySetting")
	TMap<int, EAcceptabilityEnum> AllowRace;
	
	bool IsEntityAllowed(int32 ClassId, int32 ProfId, int32 RaceId) const
	{
		if (bIsPublic) return true;
		
		EAcceptabilityEnum ClassAccept = GetClassAcceptability(ClassId);
		EAcceptabilityEnum ProfAccept = GetProfAcceptability(ProfId);
		EAcceptabilityEnum RaceAccept = GetRaceAcceptability(RaceId);
    
		if (ClassAccept == EAcceptabilityEnum::Prohibited ||
			ProfAccept == EAcceptabilityEnum::Prohibited ||
			RaceAccept == EAcceptabilityEnum::Prohibited)
		{
			return false;
		}
    
		if (ClassAccept == EAcceptabilityEnum::UnconditionallyAccept ||
			ProfAccept == EAcceptabilityEnum::UnconditionallyAccept ||
			RaceAccept == EAcceptabilityEnum::UnconditionallyAccept)
		{
			return true;
		}
    
		return ClassAccept >= EAcceptabilityEnum::NeutralNegative &&
			   ProfAccept >= EAcceptabilityEnum::NeutralNegative &&
			   RaceAccept >= EAcceptabilityEnum::NeutralNegative;
	}
	EAcceptabilityEnum GetClassAcceptability(int32 ClassId) const
	{
		return AllowClass.Contains(ClassId) ? AllowClass[ClassId] : EAcceptabilityEnum::Neutral;
	}
	EAcceptabilityEnum GetProfAcceptability(int32 ProfId) const
	{
		return AllowProf.Contains(ProfId) ? AllowProf[ProfId] : EAcceptabilityEnum::Neutral;
	}
	EAcceptabilityEnum GetRaceAcceptability(int32 RaceId) const
	{
		return AllowRace.Contains(RaceId) ? AllowRace[RaceId] : EAcceptabilityEnum::Neutral;
	}
};

USTRUCT(BlueprintType)
struct FAreaTypeTemplate : public FTableRowBase
{
	GENERATED_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/AreaType")
	FGameplayTag AreaTypeId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	FAreaAuthorityBFragment Authority;
};

USTRUCT(BlueprintType)
struct FAreaFunctionBFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Function")
	EAreaStatus Status;
	
	// [修改] 现在直接存储 Tag，不再有 int TypeId
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Function")
	FGameplayTag AreaType;
};

USTRUCT(BlueprintType)
struct FShopInformFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/ShopInform")
	float Evaluation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/ShopInform")
	TArray<int> RoomFunctionType;
};

USTRUCT(BlueprintType)
struct FAreaInstance : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	int AreaId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	FName AreaName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	int ParentScenceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	int ParentAreaId; // 0 TopArea 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	FAreaFunctionBFragment Function;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	FAreaCoverageFragment AreaCoverage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	FAreaAuthorityBFragment Authority;

	// [新增] 区域的动态标签集 (SRPG/解谜用)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area|Tags")
	FGameplayTagContainer GameplayTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	int OwnerCreatureId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	int ManagerCreatureId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area")
	TArray<int> UsableCreatureId;
	
	virtual bool InitializeAreaInform(const FAreaInstance& input)
	{
		AreaId = input.AreaId;
		ParentScenceId = input.ParentScenceId;
		ParentAreaId = input.ParentAreaId;
		AreaName = input.AreaName;
		Function = input.Function;
		AreaCoverage = input.AreaCoverage;
		Authority = input.Authority;
		GameplayTags = input.GameplayTags; // 同步 Tag
		return true;
	};

	virtual bool UpdateAreaInform(FAreaInstance& Input)
	{	
		ParentScenceId = Input.ParentScenceId;
		AreaName = Input.AreaName;
		Function = Input.Function;
		AreaCoverage = Input.AreaCoverage;
		Authority = Input.Authority;
		GameplayTags = Input.GameplayTags; // 同步 Tag
		return true;
	};
	
	void UpdateParentArea(int ParentId)
	{
		ParentAreaId = ParentId;
	}

	virtual void ResetArea()
	{
		AreaName = "null";
		Function.AreaType = FGameplayTag::EmptyTag; // 重置为空 Tag
		ParentAreaId = -1;
		AreaCoverage.SetBoundingBox(FIntVector(0), FIntVector(0));
		AreaCoverage.CoreOffsetIndex = FIntVector(0);
		GameplayTags.Reset();
	}

	// [修改] 更新函数只更新 Tag
	void UpdateAreaType(const FAreaTypeBase& input)
	{
		if (Function.AreaType == input.AreaTypeTag)
		{
			return;
		}
		Function.AreaType = input.AreaTypeTag;
	};
	
	void UpdateAreaStatus(const EAreaStatus& input)
	{
		Function.Status = input;
	};
	
	bool IsNullOwner()
	{
		return OwnerCreatureId == -1;
	};
	
	bool IsBoxInArea(FBox Input)
	{
		return AreaCoverage.BoundingBox().IsInsideOrOn(Input);
	};
	
	// [修改] 使用 Tag 匹配来判断是否为 Shop
	bool IsShop() const
	{
		static const FGameplayTag ShopTag = FGameplayTag::RequestGameplayTag(FName("Area.Type.Shop"));
		return Function.AreaType.MatchesTag(ShopTag);
	};
	
	// [修改] 使用 Tag 匹配来判断是否为子功能区
	bool IsSubFunction() const
	{
		static const FGameplayTag SubFuncTag = FGameplayTag::RequestGameplayTag(FName("Area.Type.Shop.SubFunction"));
		return Function.AreaType.MatchesTag(SubFuncTag) && ParentAreaId != -1;
	};
	
	bool IsInArea(FVector InputPosition)
	{
		return AreaCoverage.BoundingBox().IsInside(InputPosition);
	};
	
	bool IsOwner(int CharCheckId) const
	{
		return OwnerCreatureId == CharCheckId;
	};
	
	bool IsManager(int CharCheckId) const
	{
		return ManagerCreatureId == CharCheckId;
	};
};

USTRUCT(BlueprintType)
struct FAreaModificationData
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int ParentScenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int ParentAreaId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAreaStatus Status;
	// [修改] 类型改为 Tag
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag AreaType;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FAreaCoverageFragment AreaCoverage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int OwnerCreatureId = -1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int ManagerCreatureId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsValid = false; 

	FAreaModificationData()
	{
		ParentScenceId = 0;
		ParentAreaId = 0;
		AreaType = FGameplayTag::EmptyTag;
		AreaName = NAME_None;
		AreaCoverage.SetBoundingBox(FBox(ForceInitToZero)); 
		OwnerCreatureId = -1;
		ManagerCreatureId = -1;
		bIsValid = false;
	};
    
	void Reset(int InParentScenceId)
	{
		AreaName = NAME_None;
		ParentScenceId = InParentScenceId;
		AreaType = FGameplayTag::EmptyTag;
		OwnerCreatureId = -1;
		ManagerCreatureId = -1;
		bIsValid = false; 
		AreaCoverage.SetBoundingBox(FBox(ForceInitToZero));
	};

	void GetInfoByArea(FAreaInstance& Input)
	{
		AreaName = Input.AreaName;
		ParentScenceId = Input.ParentScenceId;
		ParentAreaId = Input.ParentAreaId;
		AreaType = Input.Function.AreaType;
		AreaCoverage = Input.AreaCoverage;
		OwnerCreatureId = Input.OwnerCreatureId;
		ManagerCreatureId = Input.ManagerCreatureId;
	};
	
	void SetRange(FBox InBox)
	{
		AreaCoverage.SetBoundingBox(InBox);
		bIsValid = true; 
	}
};