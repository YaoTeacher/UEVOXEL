// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataRegistry.h"
#include "Data/Area/VoxelAreaBase.h"
#include "Data/Creature/CreatureDataDefinitions.h"
#include "Data/Item/VoxelItemBase.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ConfigDataSubsystem.generated.h"

class UCreatureEvent;
struct FAreaTypeBase;
struct FVoxelItemBase;
/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UConfigDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	TObjectPtr<UDataRegistry> ItemTypeRegistry;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	TObjectPtr<UDataRegistry> AreaTypeRegistry;
	
	UPROPERTY(EditDefaultsOnly, Category = "DataResource|Creature")
	FString FormulaSearchPath = TEXT("/Game/Resource/DataRresource/Config/CreatureConfig/AttributeConfig");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataResource")
	UDataTable* ActionFlowConfig;
	// 获取各个Subsystem
    
	// 统一的数据操作
	UFUNCTION(BlueprintCallable, Category = "Data")
	bool SaveAllData(const FString& FilePath);
    
	UFUNCTION(BlueprintCallable, Category = "Data")
	bool LoadAllData(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	void LoadAllItemType();
	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	void LoadAllAreaType();
	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	void LoadAllLifeEvents();
	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	TMap<FGameplayTag,FText> GetAllAreaType();

// =============================================================
	// [新增部分 1] UI 表现配置 (DataTable -> Tag UI)
	// =============================================================
public:
	/** 对应截图中的 DT_TagUIDefConfig */
	UPROPERTY(EditDefaultsOnly, Category = "DataResource|UI")
	UDataTable* TagConfigTable;

	/** 运行时缓存：Tag -> UI配置 (用于 O(1) 快速查找名称/图标/颜色) */
	UPROPERTY(BlueprintReadOnly, Category = "Data|Cache")
	TMap<FGameplayTag, FTagUIDefinition> TagUIMap;

	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	void LoadTagUIConfig();

	// --- UI 获取接口 ---
	UFUNCTION(BlueprintCallable, Category = "Data|UI")
	FText GetTagDisplayName(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Data|UI")
	TSoftObjectPtr<UTexture2D> GetTagIcon(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Data|UI")
	FLinearColor GetTagColor(FGameplayTag Tag) const;

	// =============================================================
	// [新增部分 2] 生物系统数据 (DataRegistry -> Soft Asset)
	// =============================================================
	// --- 缓存 Tag 列表 (用于随机生成) ---
	UPROPERTY(BlueprintReadOnly, Category = "Data|Cache")
	TArray<FGameplayTag> CachedRaceTags;

	UPROPERTY(BlueprintReadOnly, Category = "Data|Cache")
	TArray<FGameplayTag> CachedProfessionTags;
    
	UPROPERTY(BlueprintReadOnly, Category = "Data|Cache")
	TArray<FGameplayTag> CachedTraitTags;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, UCreatureEvent*> CachedLifeEvents;

	/** * 运行时缓存：目标属性 Tag -> 公式定义
	 * 作用：CalculateStats 时无需遍历数组，直接查 Map
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Data|Cache")
	TMap<FGameplayTag, FDerivedAttributeDef> CachedFormulaMap;
	UFUNCTION(BlueprintCallable, Category = "InitializeGame")

	
	/** 辅助：将单个资产合并进缓存 */
	
	void MergeFormulasToCache(const UGlobalAttributeFormulas* Asset);
	/** 扫描指定目录并合并所有公式 */
	void ScanAndLoadAllFormulas();

	/** 扫描注册表并缓存所有可用的 Tag */
	UFUNCTION(BlueprintCallable, Category = "InitializeGame")
	void CacheRegistryTags();

	// --- 模板获取接口 ---
	/** 获取种族模板 */
	UFUNCTION(BlueprintCallable, Category = "Data|Creature")
	UCreatureRaceTemplate* GetRaceTemplate(FGameplayTag RaceTag);

	/** 获取职业模板 */
	UFUNCTION(BlueprintCallable, Category = "Data|Creature")
	UCreatureProfessionTemplate* GetProfessionTemplate(FGameplayTag ProfessionTag);

	/** 获取特质定义 */
	UFUNCTION(BlueprintCallable, Category = "Data|Creature")
	UCreatureTraitDefinition* GetTraitDefinition(FGameplayTag TraitTag);

	UFUNCTION(BlueprintCallable, Category = "Data|Creature")
	TMap<FGameplayTag,UCreatureTraitDefinition*> GetAllTraitDefinitions();

	/** 获取事件定义 */
	UFUNCTION(BlueprintCallable, Category = "Config|Events")
	UCreatureEvent* GetLifeEventById(const FPrimaryAssetId Id) const;
    
	// (可选) 获取所有事件，用于生成时的随机池
	UFUNCTION(BlueprintCallable, Category = "Config|Events")
	TArray<UCreatureEvent*> GetAllLifeEvents() const;
	// --- 随机辅助 ---
	UFUNCTION(BlueprintCallable, Category = "Data|Creature")
	FGameplayTag GetRandomRaceTag();



	// =============================================================
	// 数据存储
	// =============================================================

	/**
	 * ItemMap: 物品类型缓存映射 (ItemId -> ItemData)
	 *
	 * 使用 ItemId (int32) 作为 key，替代原来的 FGameplayTag。
	 * 原因：实例层海量存储时 int32 比 FGameplayTag 更省内存、比较更快。
	 * 分类查询仍可通过 ItemTag.MatchesTag 实现（见 GetItemTypeArrayByCategory）。
	 *
	 * 存储非const指针的原因说明：
	 * DataRegistry的GetCachedItem()返回const指针，但我们需要支持模板类型转换
	 * （GetItemTypeById<T>()需要将FVoxelItemBase*转换为具体的子类型指针）。
	 *
	 * 这是类型安全的，原因如下：
	 * - FVoxelItemBase及其派生类中的所有字段都是BlueprintReadOnly，设计上就是只读配置数据
	 * - 我们从不修改DataRegistry管理的内存，ItemMap只是便利的缓存层
	 * - 数据的实际生命周期由DataRegistry管理，ItemMap不拥有这些数据
	 * - const_cast只在LoadAllItemType()中进行一次，从DataRegistry的const指针转为内部缓存的非const指针
	 *
	 * 注意：虽然指针是非const的，但代码中应该始终将ItemMap中的数据视为只读配置
	 */
	TMap<int32,FVoxelItemBase*> ItemMap;

	/** FullId(Source:IdentityName) -> RuntimeHandle 映射，身份字符串到运行时句柄 */
	TMap<FString, int32> FullIdToHandle;

	/** AreaMap: 区域类型缓存映射 (Tag -> AreaData) */
	TMap<FGameplayTag,const FAreaTypeBase*> AreaMap;

	// =============================================================
	// [Template System] 运行时模板
	// =============================================================

	/** 持有运行时模板的所有权（ItemMap 指向这里的指针） */
	UPROPERTY(Transient)
	TMap<int32, FVoxelTemplate> RuntimeTemplates;

	/** 模板 ID 起始值 */
	int32 NextTemplateId = 200000;

	/** 启动时扫描 Saved/Templates 目录 */
	UFUNCTION(BlueprintCallable, Category = "InitializeGame|Template")
	void ScanAndLoadAllTemplates();

	/** 注册一个运行时模板 */
	int32 RegisterRuntimeTemplate(FVoxelTemplate&& T);

	/** 注销模板 */
	UFUNCTION(BlueprintCallable, Category = "Template")
	bool UnregisterTemplate(int32 HandleId);

	/** 分配新的模板 ItemId */
	int32 AllocateNewTemplateId() { return NextTemplateId++; }

	/** 根据 Guid 查找模板 */
	const FVoxelTemplate* FindTemplateByGuid(const FGuid& Guid) const;

	/** 获取所有模板 ID */
	UFUNCTION(BlueprintCallable, Category = "Template")
	TArray<int32> GetAllTemplateIds() const;

	/** 根据运行时句柄查找模板（仅查 RuntimeTemplates） */
	FVoxelTemplate* GetTemplateById(int32 Handle);

	/** 获取所有模板指针（模板库列表查询，O(模板数)） */
	TArray<FVoxelTemplate*> GetAllTemplates();

	/**
	 * 统一查找入口：先查配置物品(HandleToItem)，未命中再查模板(RuntimeTemplates)。
	 * 战斗/通用使用管线用此接口，按返回实体的 ItemTag 做类型路由。
	 */
	FVoxelItemBase* FindByHandle(int32 Handle);

	/** 由完整身份ID查运行时句柄；未命中返回 -1 */
	int32 GetHandleByFullId(const FString& FullId) const;

	/** 由运行时句柄查完整身份ID；未命中返回空字符串 */
	FString GetFullIdByHandle(int32 Handle) const;

	/** 计算身份字符串对应的运行时句柄（决定性 hash） */
	static int32 ComputeHandle(const FString& FullId)
	{
		return static_cast<int32>(GetTypeHash(FullId));
	}

public:

	
	template<typename T>
	 T* GetItemTypeById(const int32 HandleId)
	{
		static_assert(std::is_base_of_v<FVoxelItemBase, T>,
				  "T must be a FVoxelItemBase-derived struct");

		if (!ItemMap.Contains(HandleId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item with ID %d not found in WorldItemType"), HandleId);
			return nullptr;
		}
		// 检查 TMap 是否包含该 ID
		FVoxelItemBase**  ItemPtr = ItemMap.Find(HandleId);
		// 获取项目
		FVoxelItemBase* Item = *ItemPtr;

		// 检查项目是否为空
		if (!Item)
		{
			UE_LOG(LogTemp, Warning, TEXT("Item with ID %d is null"), HandleId);
			return nullptr;
		}

		// 直接进行类型转换
		return static_cast<T*>(Item);
	};

	template<typename T>
	TArray<T*> GetItemTypeArrayByCategory(const FGameplayTag CategoryTag)
	{
		// 1. 静态断言检查类型安全
		static_assert(std::is_base_of_v<FVoxelItemBase, T>,
				"T must be a FVoxelItemBase-derived struct");

		TArray<T*> ResultArray;

		// 2. 基础检查
		if (ItemMap.Num() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("WorldItemType is empty"));
			return ResultArray;
		}

		// 3. 遍历 Map
		for (const auto& Pair : ItemMap)
		{
			FVoxelItemBase* Item = Pair.Value;

			// 空指针检查
			if (!Item)
			{
				continue;
			}

			// 4. 标签匹配逻辑
			// MatchesTag: 检查 ItemTag 是否包含 CategoryTag。
			// 例如：如果 ItemTag 是 "Item.Type.Block.Solid.Stone"，传入 "Item.Type.Block" 会返回 true。
			if (Item->ItemTag.MatchesTag(CategoryTag))
			{
				// 5. 直接进行类型转换
				T* TypedItem = static_cast<T*>(Item);

				if (TypedItem)
				{
					ResultArray.Add(TypedItem);
				}
			}
		}

		return ResultArray;
	}
	
	// template<typename T>
	// TArray<T*> GetItemTypeArrayByType(const EVoxelItemType ExpectedType)
	// {
	// 	static_assert(std::is_base_of_v<FVoxelItemBase, T>,
	// 			  "T must be a FVoxelItemBase-derived struct");
	// 	TArray<T*> ResultArray;
 //        
	// 	// 检查 TMap 是否为空
	// 	if (ItemMap.Num() <= 0)
	// 	{
	// 		UE_LOG(LogTemp, Warning, TEXT("WorldItemType is empty"));
	// 		return ResultArray;
	// 	}
 //        
	// 	// 遍历所有项目
	// 	for (const auto& Pair : ItemMap)
	// 	{
	// 		// 获取项目
	// 		const FVoxelItemBase* Item = Pair.Value;
 //            
	// 		// 检查项目是否为空
	// 		if (Item==nullptr)
	// 		{
	// 			UE_LOG(LogTemp, Warning, TEXT("Item with ID %s is null"), *Pair.Key.ToString());
	// 			continue;
	// 		}
 //            
	// 		// 检查项目类型
	// 		if (Item->ItemType == ExpectedType)
	// 		{
	// 			// 类型转换并添加到结果数组
	// 			T* TypedItem = const_cast<T*>(static_cast<const T*>(Item));
	// 			if (TypedItem)
	// 			{
	// 				ResultArray.Add(TypedItem);
	// 			}
	// 		}
	// 	}
 //        
	// 	return ResultArray;
	// }

	const FAreaTypeBase* GetAreaTypeByTag(const FGameplayTag AreaId)
	{
		if (AreaMap.Contains(AreaId))
		{
			return AreaMap[AreaId];
		}
		return nullptr;
	};

	/**
	 * 区域类型 Tag → 显示名（全工程唯一实现，UI 各处只调不写）
	 * 查到配置且 DisplayName 非空时返回 DisplayName；
	 * 查不到或为空时回退显示 Tag 原始名（Area.Type.Shop），避免空白
	 */
	UFUNCTION(BlueprintCallable, Category = "Data|UI")
	FText GetAreaTypeDisplayName(const FGameplayTag AreaTypeTag) const;

	const FAreaTypeTemplate* GetAreaTypeTemplateById(const FGameplayTag AreaId)
	{
		if (AreaMap.Contains(AreaId))
		{

		}
		return nullptr;
	}

	/**
	 * 按父级 Tag 获取区域类型数组
	 * 使用 MatchesTag 匹配层级关系
	 * 例如：传入 Area.Type 会返回所有子类型
	 */
	TArray<const FAreaTypeBase*> GetAreaTypeArrayByTag(const FGameplayTag ParentTag)
	{
		TArray<const FAreaTypeBase*> ResultArray;

		if (AreaMap.Num() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AreaMap is empty"));
			return ResultArray;
		}

		for (const auto& Pair : AreaMap)
		{
			const FAreaTypeBase* AreaType = Pair.Value;
			if (!AreaType)
			{
				continue;
			}

			// MatchesTag: 检查 AreaTypeTag 是否包含 ParentTag
			if (AreaType->AreaTypeTag.MatchesTag(ParentTag))
			{
				ResultArray.Add(AreaType);
			}
		}

		return ResultArray;
	}

	/**
	 * 获取所有区域类型（包含抽象类型，抽象类型可用于重叠组合）
	 */
	TArray<const FAreaTypeBase*> GetAllAreaTypes()
	{
		TArray<const FAreaTypeBase*> ResultArray;

		for (const auto& Pair : AreaMap)
		{
			const FAreaTypeBase* AreaType = Pair.Value;
			if (AreaType)
			{
				ResultArray.Add(AreaType);
			}
		}

		return ResultArray;
	}

private:
	/** 核心泛型辅助函数：从 Registry 获取包装结构体并加载资产 */
	template <typename TRegistryItem, typename TAssetType>
	TAssetType* GetAssetFromRegistry(FName RegistryType, FGameplayTag Tag, TSoftObjectPtr<TAssetType> TRegistryItem::* AssetPtrMember);
};
