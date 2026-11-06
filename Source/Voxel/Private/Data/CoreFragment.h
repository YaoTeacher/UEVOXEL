#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "CoreFragment.generated.h"


/**
 * FVoxelTextureSet - 统一 PBR 贴图集
 *
 * 方块/家具/人物共用，存储一个面的完整 PBR 贴图引用。
 * - 方块：通过 TextureRegistry 查询索引，写入顶点色 Alpha，采样 Texture2DArray
 * - 家具/人物：直接通过 MID 设置 BaseColorMap/NormalMap/ORMMap 参数
 *
 * 空的贴图引用表示该方块/家具不使用该贴图通道，
 * TextureRegistry 会用默认贴图（白色 BaseColor、蓝色 Normal、白色 ORM）填充。
 */
USTRUCT(BlueprintType)
struct FVoxelTextureSet
{
	GENERATED_USTRUCT_BODY()

	/** 基础色贴图（sRGB） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Texture|PBR")
	TSoftObjectPtr<UTexture2D> BaseColorMap;

	/** 法线贴图（sRGB，可选） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Texture|PBR")
	TSoftObjectPtr<UTexture2D> NormalMap;

	/** ORM 贴图（Linear，R=AO, G=Roughness, B=Metallic，可选） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Texture|PBR")
	TSoftObjectPtr<UTexture2D> ORMMap;

	/** 是否为空（三个贴图都未配置） */
	bool IsEmpty() const
	{
		return !BaseColorMap.IsValid() && !NormalMap.IsValid() && !ORMMap.IsValid();
	}

	/**
	 * 计算哈希值用于去重（基于三个贴图的资产路径）
	 * 相同的贴图组合会得到相同的哈希，从而共享同一个 TextureArray slice
	 */
	uint32 GetHash() const
	{
		uint32 Hash = 0;
		if (BaseColorMap.IsValid()) Hash = HashCombine(Hash, GetTypeHash(BaseColorMap.ToSoftObjectPath().GetAssetPath()));
		if (NormalMap.IsValid())    Hash = HashCombine(Hash, GetTypeHash(NormalMap.ToSoftObjectPath().GetAssetPath()));
		if (ORMMap.IsValid())       Hash = HashCombine(Hash, GetTypeHash(ORMMap.ToSoftObjectPath().GetAssetPath()));
		return Hash;
	}

	bool operator==(const FVoxelTextureSet& Other) const
	{
		return BaseColorMap == Other.BaseColorMap
			&& NormalMap == Other.NormalMap
			&& ORMMap == Other.ORMMap;
	}
};

// 为 TMap 提供 HashType 便于去重
FORCEINLINE uint32 GetTypeHash(const FVoxelTextureSet& TS)
{
	return TS.GetHash();
}

USTRUCT(BlueprintType)
struct  FVoxelVisualFragment
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** 统一 PBR 贴图集（家具/人物用，通过 M_Furniture 的 MID 设置参数） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data|Texture")
	FVoxelTextureSet TextureSet;

};
