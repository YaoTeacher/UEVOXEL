// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/Enum/GameStateEnum.h"
#include "Data/CoreFragment.h"
#include "Data/Area/AreaEnums.h"
#include "Data/Creature/CreatureEnums.h"
#include "Engine/DataTable.h"
#include "VoxelItemBase.generated.h"

class AVoxelAreaBaseInstance;
/**
 * 
 */
// UENUM(BlueprintType)
// enum class EVoxelItemType : uint8
// {
// 	None            UMETA(DisplayName = "未分类"),
//    BuildBlock      UMETA(DisplayName = "建筑方块"),
//    BuildItems      UMETA(DisplayName = "建筑物品"),
//    ConsumableItems      UMETA(DisplayName = "消耗品"),
//    StaticItems     UMETA(DisplayName = "静态道具"),
//    EquipmentItems       UMETA(DisplayName = "装备"),
//    Skills		UMETA(DisplayName = "技能物品"),
//     
//    // 预留扩展位
//    _MAX UMETA(Hidden)
// };
//
// // 物品功能类型 - 按功能角色划分
// UENUM(BlueprintType, Meta = (DisplayName = "功能类型"))
// enum class EVoxelItemFunction : uint8
// {
// 	None            UMETA(DisplayName = "无功能"),
// 	BarCounter      UMETA(DisplayName = "吧台"),
// 	Table           UMETA(DisplayName = "桌子"),
// 	Chair           UMETA(DisplayName = "椅子"),
// 	EntryPoint      UMETA(DisplayName = "入口"),
// 	Bed             UMETA(DisplayName = "床"),
// 	Bathtub         UMETA(DisplayName = "浴缸"),
// 	Shelf           UMETA(DisplayName = "货架"),
// 	Storage         UMETA(DisplayName = "储物柜"),
// 	CookingStation  UMETA(DisplayName = "烹饪台"),
//     
// 	// 预留扩展位
// 	_MAX UMETA(Hidden)
// };

// USTRUCT(BlueprintType)
// struct FVoxelNumericFragment
// {
// 	GENERATED_USTRUCT_BODY();
//
//
//
//
//
// };

USTRUCT(BlueprintType)
struct FVoxelItemCirculateDataFragment
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	bool IsCirculatable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	bool IsDestructible;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	int MaxStackCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	float Weight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	float Value;



};

/**
 * FVoxelBlockTextureSetup - 方块3面贴图配置（Top/Side/Bottom）
 *
 * 仅需配置 3 个贴图集：顶面、侧面（4面共用）、底面。
 * 运行时由 UVoxelTextureRegistry 收集去重，动态生成 Texture2DArray 并分配索引。
 *
 * 渲染层 TextureIndices[6] 保持 6 元素，侧面索引统一映射到 Side：
 *   [0]=Side [1]=Side [2]=Top [3]=Bottom [4]=Side [5]=Side
 */
USTRUCT(BlueprintType)
struct FVoxelBlockTextureSetup
{
	GENERATED_USTRUCT_BODY()

	/** 顶面（+Z） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define|Texture")
	FVoxelTextureSet Top;

	/** 侧面（4面共用：-X/+X/-Y/+Y） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define|Texture")
	FVoxelTextureSet Side;

	/** 底面（-Z） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define|Texture")
	FVoxelTextureSet Bottom;

	/** 按法线方向获取面贴图集 */
	const FVoxelTextureSet& GetFaceByNormal(const FVector& Normal) const
	{
		if (Normal == FVector::UpVector)       return Top;
		if (Normal == FVector::DownVector)     return Bottom;
		return Side;
	}
};

USTRUCT(BlueprintType)
struct FVoxelBlockItemFragment
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	bool IsSolid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	bool IsTransparent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	bool IsRenderNeibor;

	/** 6面 PBR 贴图配置（替代旧的6个int索引，运行时由 TextureRegistry 动态分配索引） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	FVoxelBlockTextureSetup TextureSetup;
};

USTRUCT(BlueprintType)
struct FVoxelPhysicFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	float Roughness;


};


USTRUCT(BlueprintType)
struct FVoxelItemBasicTendencyFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	TMap<ECreatureRace,float> RaceTendencyValue;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	TMap<ECreatureProfession,float> ProfessionTendencyValue;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	TMap<ECreatureClass,float> ClassTendencyValue;
};



USTRUCT(BlueprintType)
struct FVoxelItemInstanceCore
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	int HandleId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	FGameplayTag ItemTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	int Quantity;
	
	
};

USTRUCT(BlueprintType)
struct FVoxelInstanceContainerFragment
{
	GENERATED_USTRUCT_BODY();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	int MaxCell;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	TMap<int,int> ContainerList; 
	
};

USTRUCT(BlueprintType)
struct FVoxelItemInstanceBase: public FVoxelItemInstanceCore
{
	GENERATED_BODY()

public:
// 	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item")
// 	FVoxelItemInstanceCore InstanceCore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	EActorDirection Direction=EActorDirection::Forward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	TSoftObjectPtr<AVoxelAreaBaseInstance> ParentArea;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	TArray<FVoxelInstanceContainerFragment> StorageItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data/Item/Instance")
	TMap<int,bool> SeatOccupation;


	void ResetInform()
	{
		ItemTag=FGameplayTag::EmptyTag;
		Quantity=0;
		Direction = EActorDirection::Forward;
		ParentArea=nullptr;
	}
	
};

USTRUCT(BlueprintType)
struct FVoxelItemBasicFunctionFragment
{
	GENERATED_USTRUCT_BODY();


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDemandEnum LinkedDemand;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EffectValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DurationSec;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SusTainSec;
	
};

USTRUCT(BlueprintType)
struct FVoxelItemUsageFragment
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	bool IsSustainableAfterUse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	bool IsDestroyAfterUse;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	int MaxUserNum = 1;



	// UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item")
	// TArray<ECreatureActionType> FunctionList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/ItemDefine")
	TMap<ECreatureActionType,FVoxelItemBasicFunctionFragment> FunctionEffectList;
	
	

};



USTRUCT(BlueprintType)
struct FVoxelItemCoreBase : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 【身份-来源】命名空间来源段：core / dlc_xxx / mod_xxx
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Identity")
	FString Source = TEXT("core");

	// 【身份-名称】身份名称段：block.stone / item.chair / tmpl.<guid>
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Identity")
	FString IdentityName;

	// 【运行时句柄】= GetTypeHash(GetFullId())，启动时由 ConfigDataSubsystem 计算填充。
	// 语义=运行时查找句柄（地形存储/手持/EditRequest 均用此值），不再由配置表手填。
	UPROPERTY(BlueprintReadOnly, Category = "Data/Item/Core")
	int HandleId = -1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	FGameplayTag ItemTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	int ParentHandleId; //0为主道具 若有为Sub变种
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	FText ItemDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Core")
	FVoxelVisualFragment AssetFragment;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item")
	TArray<FIntVector> OccupiedBlocks;

	// 完整身份 ID：Source:IdentityName（如 "core:block.stone"）
	FString GetFullId() const { return Source + TEXT(":") + IdentityName; }
};

USTRUCT(BlueprintType)
struct FVoxelItemBase : public FVoxelItemCoreBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	FVoxelItemCirculateDataFragment CirculateDataFragment;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	FVoxelItemUsageFragment BasicFunctionFragment;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	bool IsPlaceable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	TArray<int> LimitPlace;
	


	
};

USTRUCT(BlueprintType)
struct FVoxelNormalItemBase : public FVoxelItemBase 
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Data/Item/Define")
	TMap<int,int> MaxContainerForEach;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item/Define")
	FVector Volume;
	
	bool HasContainer {MaxContainerForEach.Num()!=0};
	
	FIntVector GetVoxelVolume(float BlockLength) const
	{
		// 确保 BlockLength 大于 0
		if (BlockLength <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("BlockLength must be greater than 0!"));
			return FIntVector::ZeroValue;
		}

		// 向上取整计算每个维度的体素数量
		int32 VoxelX = FMath::CeilToInt(Volume.X / BlockLength);
		int32 VoxelY = -FMath::CeilToInt(Volume.Y / BlockLength);
		int32 VoxelZ = FMath::CeilToInt(Volume.Z / BlockLength);

		return FIntVector(VoxelX, VoxelY, VoxelZ);
	}
};

USTRUCT(BlueprintType)
struct FVoxelSkillBase  : public FVoxelItemBase
{
	GENERATED_BODY()

public:
	
	//EVoxelItemType ItemType= EVoxelItemType::Skills;
	
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data/Item")
	// UTexture2D* Icon;
};

// =============================================================
// FVoxelInstanceEntry - 通用实例数据（方块/家具/模板共用）
//
// 身份字段（持久化权威）：FullId
//   - 完整身份串 "core:block.stone" / "core:item.chair.wooden"
//   - 模板文件/存档只写此字段；Air 用 "core:block.air" 表示
//
// 运行时句柄（内存缓存，不持久化）：HandleId
//   - = GetTypeHash(FullId)，加载后由 ConfigDataSubsystem 解析填充
//   - 内存层使用，提升 TMap 查找性能
//   - 特殊值：EBlockId::Air=0, EBlockId::Invalid=-1
// =============================================================
USTRUCT(BlueprintType)
struct FVoxelInstanceEntry
{
	GENERATED_BODY()

	// 【身份-持久化】完整身份 ID（"core:block.stone" / "core:item.chair.wooden"）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FString FullId;

	// 【运行时句柄】= GetTypeHash(FullId)，加载后由 ConfigDataSubsystem 解析填充
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance|Runtime")
	int32 HandleId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	EActorDirection Direction = EActorDirection::Forward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FString CustomLabel;

	// 运行时业务状态（模板序列化时跳过）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance|Runtime")
	TArray<FVoxelInstanceContainerFragment> StorageItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance|Runtime")
	TMap<int32, bool> SeatOccupation;

	// FullId 是否有效（非空）
	FORCEINLINE bool IsValid() const { return !FullId.IsEmpty(); }
	// 是否是 Air
	FORCEINLINE bool IsAir()   const { return FullId == TEXT("core:block.air"); }

	// 创建方块条目（FullId 必填，HandleId 可选-加载后由 ConfigSys 解析）
	static FVoxelInstanceEntry MakeBlock(const FString& InFullId, int32 InHandleId = -1)
	{
		FVoxelInstanceEntry E;
		E.FullId = InFullId;
		E.HandleId = InHandleId;
		return E;
	}
	// 创建家具条目
	static FVoxelInstanceEntry MakeFurniture(const FString& InFullId, EActorDirection InDir, int32 InHandleId = -1)
	{
		FVoxelInstanceEntry E;
		E.FullId = InFullId;
		E.HandleId = InHandleId;
		E.Direction = InDir;
		return E;
	}

	bool operator==(const FVoxelInstanceEntry& Other) const
	{
		return FullId == Other.FullId && Direction == Other.Direction;
	}
};

// =============================================================
// FVoxelTemplate - 模板配置（继承 FVoxelItemBase）
// =============================================================
USTRUCT(BlueprintType)
struct FVoxelTemplate : public FVoxelItemBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	FGuid TemplateGuid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	FString FormatVersion = TEXT("1.1");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	FIntVector Size = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	FIntVector Anchor = FIntVector::ZeroValue;

	// 统一内容：相对坐标 → 实例条目
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template")
	TMap<FIntVector, FVoxelInstanceEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Meta")
	FString Author;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Meta")
	FString AuthorPlayerGuid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Meta")
	FDateTime CreatedTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Meta")
	FDateTime ModifiedTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Template|Meta")
	TArray<FString> Tags;

	UPROPERTY(Transient)
	FString SourceFilePath;
};

// 创建模板时的元信息输入
USTRUCT(BlueprintType)
struct FTemplateMeta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template|Meta")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template|Meta")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template|Meta")
	FString Author;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template|Meta")
	FGameplayTag CategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template|Meta")
	TArray<FString> Tags;
};


