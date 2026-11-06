#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/CoreFragment.h"
#include "VoxelTextureRegistry.generated.h"

class UTexture2D;
class UTexture2DArray;
struct FVoxelItemBase;

/**
 * UVoxelTextureRegistry - 贴图注册中心
 *
 * 运行时动态生成 Texture2DArray，统一管理方块/家具/人物的贴图索引。
 *
 * 工作流程：
 * 1. Initialize() 遍历所有方块物品配置，收集不重复的 FVoxelTextureSet
 * 2. 为每个贴图集分配索引（0, 1, 2, ...）
 * 3. 生成 3 个 Texture2DArray（BaseColor / Normal / ORM）
 * 4. GetTextureIndex() 供 BuildChunkContext 查询，写入顶点色 Alpha
 * 5. GetBaseColorArray() 等供材质 MID 设置 TextureArray 参数
 *
 * 空的贴图引用用默认贴图填充：
 * - BaseColor: 白色（255,255,255,255）
 * - Normal: 蓝色（128,128,255,255）= tangent space 朝上
 * - ORM: 白色（255,255,0,255）= AO=1, Roughness=1, Metallic=0
 */
UCLASS(Blueprintable, BlueprintType)
class UVoxelTextureRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 扫描所有方块物品配置，收集贴图集，生成 Texture2DArray
	 * 必须在 ConfigDataSubsystem.LoadAllItemType() 之后调用
	 * @param ItemMap 物品配置表（来自 ConfigDataSubsystem）
	 */
	void BuildTextureArrays(const TMap<int32, FVoxelItemBase*>& ItemMap);

	/**
	 * 查询贴图集对应的 TextureArray 索引
	 * @param TextureSet 贴图集
	 * @return 数组中的 slice 索引（未注册时返回 0 = 默认贴图）
	 */
	int32 GetTextureIndex(const FVoxelTextureSet& TextureSet) const;

	/** 获取 BaseColor 贴图数组 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Texture")
	UTexture2DArray* GetBaseColorArray() const { return BaseColorArray; }

	/** 获取 Normal 贴图数组 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Texture")
	UTexture2DArray* GetNormalArray() const { return NormalArray; }

	/** 获取 ORM 贴图数组 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Texture")
	UTexture2DArray* GetORMArray() const { return ORMArray; }

	/** 是否已完成初始化 */
	bool IsReady() const { return bIsInitialized; }

private:
	/** 贴图集哈希 → 数组索引 */
	TMap<uint32, int32> TextureSetToIndex;

	/** 运行时生成的 3 个贴图数组 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2DArray> BaseColorArray;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2DArray> NormalArray;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2DArray> ORMArray;

	/** 默认贴图（填充空位） */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultWhiteTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultNormalTexture;

	bool bIsInitialized = false;

	/** 创建默认贴图（白色 BaseColor/ORM、蓝色 Normal） */
	void CreateDefaultTextures();

	/** 从贴图集列表构建 Texture2DArray */
	bool BuildArrayFromTextures(
		const TArray<UTexture2D*>& SourceTextures,
		UObject* Outer,
		const FString& ArrayName,
		TObjectPtr<UTexture2DArray>& OutArray);

	/** 收集所有方块的不重复贴图集 */
	void CollectUniqueTextureSets(
		const TMap<int32, FVoxelItemBase*>& ItemMap,
		TArray<FVoxelTextureSet>& OutUniqueSets,
		TMap<uint32, int32>& OutHashToIndex);
};
