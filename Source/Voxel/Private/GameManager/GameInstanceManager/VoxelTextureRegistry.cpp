#include "GameManager/GameInstanceManager/VoxelTextureRegistry.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Data/Item/VoxelItemBase.h"
#include "Item/VoxelBlockItemBase.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Core/CoreTags.h"
#include "TextureCompiler.h"

void UVoxelTextureRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CreateDefaultTextures();
	UE_LOG(LogTemp, Log, TEXT("[TextureRegistry] Initialized. Default textures created."));
}

void UVoxelTextureRegistry::Deinitialize()
{
	Super::Deinitialize();
	TextureSetToIndex.Empty();
	UE_LOG(LogTemp, Log, TEXT("[TextureRegistry] Deinitialized."));
}

void UVoxelTextureRegistry::CreateDefaultTextures()
{
	// 默认白色贴图（BaseColor / ORM 用）
	if (!DefaultWhiteTexture)
	{
		DefaultWhiteTexture = UTexture2D::CreateTransient(64, 64, PF_B8G8R8A8, TEXT("DefaultWhiteTexture"));
		if (DefaultWhiteTexture)
		{
			DefaultWhiteTexture->CompressionSettings = TC_Default;
			DefaultWhiteTexture->SRGB = true;
			// 填充白色
			uint8* MipData = static_cast<uint8*>(DefaultWhiteTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			FMemory::Memset(MipData, 255, 64 * 64 * 4);
			DefaultWhiteTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
			DefaultWhiteTexture->UpdateResource();
		}
	}

	// 默认法线贴图（蓝色 = tangent space 朝上）
	if (!DefaultNormalTexture)
	{
		DefaultNormalTexture = UTexture2D::CreateTransient(64, 64, PF_B8G8R8A8, TEXT("DefaultNormalTexture"));
		if (DefaultNormalTexture)
		{
			DefaultNormalTexture->CompressionSettings = TC_Normalmap;
			DefaultNormalTexture->SRGB = true;
			// 填充蓝色 (B=255, G=128, R=128, A=255) = (0,0,1) 法线
			uint8* MipData = static_cast<uint8*>(DefaultNormalTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
			for (int32 i = 0; i < 64 * 64; ++i)
			{
				MipData[i * 4 + 0] = 255; // B
				MipData[i * 4 + 1] = 128; // G
				MipData[i * 4 + 2] = 128; // R
				MipData[i * 4 + 3] = 255; // A
			}
			DefaultNormalTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
			DefaultNormalTexture->UpdateResource();
		}
	}
}

void UVoxelTextureRegistry::CollectUniqueTextureSets(
	const TMap<int32, FVoxelItemBase*>& ItemMap,
	TArray<FVoxelTextureSet>& OutUniqueSets,
	TMap<uint32, int32>& OutHashToIndex)
{
	for (const auto& Elem : ItemMap)
	{
		const FVoxelItemBase* Item = Elem.Value;
		if (!Item || !Item->ItemTag.MatchesTag(FCoreTags::Item_Type_Block))
		{
			continue;
		}

		const FVoxelBlockItemBase* BlockItem = static_cast<const FVoxelBlockItemBase*>(Item);
		const FVoxelBlockTextureSetup& Setup = BlockItem->BlockFragment.TextureSetup;

		// 收集 3 个面的贴图集（Top/Side/Bottom）
		const FVoxelTextureSet* Faces[] = { &Setup.Top, &Setup.Side, &Setup.Bottom };
		for (int32 Face = 0; Face < 3; ++Face)
		{
			const FVoxelTextureSet& TS = *Faces[Face];
			const uint32 Hash = TS.GetHash();

			if (!OutHashToIndex.Contains(Hash))
			{
				int32 NewIndex = OutUniqueSets.Num();
				OutHashToIndex.Add(Hash, NewIndex);
				OutUniqueSets.Add(TS);
			}
		}
	}
}

void UVoxelTextureRegistry::BuildTextureArrays(const TMap<int32, FVoxelItemBase*>& ItemMap)
{
	// 1. 收集所有不重复的贴图集
	TArray<FVoxelTextureSet> UniqueSets;
	TextureSetToIndex.Empty();

	CollectUniqueTextureSets(ItemMap, UniqueSets, TextureSetToIndex);

	if (UniqueSets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TextureRegistry] No block textures found. Creating empty arrays."));
		// 添加一个默认贴图集
		UniqueSets.Add(FVoxelTextureSet());
		TextureSetToIndex.Add(0, 0);
	}

	UE_LOG(LogTemp, Log, TEXT("[TextureRegistry] Collected %d unique texture sets."), UniqueSets.Num());
	for (int32 i = 0; i < UniqueSets.Num(); ++i)
	{
		const FVoxelTextureSet& TS = UniqueSets[i];
		UE_LOG(LogTemp, Log, TEXT("[TextureRegistry]   [%d] Hash=%u BaseColor=%s"),
			i, TS.GetHash(),
			TS.BaseColorMap.IsValid() ? *TS.BaseColorMap.ToSoftObjectPath().ToString() : TEXT("None"));
	}

	// 2. 准备 3 组源贴图数组
	TArray<UTexture2D*> BaseColorSources;
	TArray<UTexture2D*> NormalSources;
	TArray<UTexture2D*> ORMSources;

	BaseColorSources.Reserve(UniqueSets.Num());
	NormalSources.Reserve(UniqueSets.Num());
	ORMSources.Reserve(UniqueSets.Num());

	for (const FVoxelTextureSet& TS : UniqueSets)
	{
		// BaseColor: 有则加载，无则用默认白色
		if (TS.BaseColorMap.IsValid())
		{
			UTexture2D* Tex = TS.BaseColorMap.LoadSynchronous();
			BaseColorSources.Add(Tex ? Tex : DefaultWhiteTexture.Get());
		}
		else
		{
			BaseColorSources.Add(DefaultWhiteTexture.Get());
		}

		// Normal: 有则加载，无则用默认法线
		if (TS.NormalMap.IsValid())
		{
			UTexture2D* Tex = TS.NormalMap.LoadSynchronous();
			NormalSources.Add(Tex ? Tex : DefaultNormalTexture.Get());
		}
		else
		{
			NormalSources.Add(DefaultNormalTexture.Get());
		}

		// ORM: 有则加载，无则用默认白色
		if (TS.ORMMap.IsValid())
		{
			UTexture2D* Tex = TS.ORMMap.LoadSynchronous();
			ORMSources.Add(Tex ? Tex : DefaultWhiteTexture.Get());
		}
		else
		{
			ORMSources.Add(DefaultWhiteTexture.Get());
		}
	}

	// 3. 构建 3 个 Texture2DArray
#if WITH_EDITOR
	if (!BuildArrayFromTextures(BaseColorSources, this, TEXT("VoxelBaseColorArray"), BaseColorArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] Failed to build BaseColorArray"));
	}
	if (!BuildArrayFromTextures(NormalSources, this, TEXT("VoxelNormalArray"), NormalArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] Failed to build NormalArray"));
	}
	if (!BuildArrayFromTextures(ORMSources, this, TEXT("VoxelORMArray"), ORMArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] Failed to build ORMArray"));
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("[TextureRegistry] Texture2DArray creation is only supported in editor builds. Arrays will be null in shipping."));
#endif

	bIsInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("[TextureRegistry] Build complete. BaseColor=%d slices, Normal=%d slices, ORM=%d slices."),
		BaseColorArray ? BaseColorArray->GetArraySize() : 0,
		NormalArray ? NormalArray->GetArraySize() : 0,
		ORMArray ? ORMArray->GetArraySize() : 0);
}

#if WITH_EDITOR
bool UVoxelTextureRegistry::BuildArrayFromTextures(
	const TArray<UTexture2D*>& SourceTextures,
	UObject* Outer,
	const FString& ArrayName,
	TObjectPtr<UTexture2DArray>& OutArray)
{
	if (SourceTextures.Num() == 0)
	{
		return false;
	}

	// 检查所有贴图尺寸是否一致
	int32 SizeX = SourceTextures[0]->GetSizeX();
	int32 SizeY = SourceTextures[0]->GetSizeY();
	for (int32 i = 1; i < SourceTextures.Num(); ++i)
	{
		if (SourceTextures[i]->GetSizeX() != SizeX || SourceTextures[i]->GetSizeY() != SizeY)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[TextureRegistry] Texture size mismatch in %s: Tex[0]=%dx%d, Tex[%d]=%dx%d. All slices must be same size."),
				*ArrayName, SizeX, SizeY, i, SourceTextures[i]->GetSizeX(), SourceTextures[i]->GetSizeY());
			return false;
		}
	}

	// 创建 Texture2DArray
	UTexture2DArray* NewArray = NewObject<UTexture2DArray>(Outer, *ArrayName, RF_Transient);
	if (!NewArray)
	{
		UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] Failed to create UTexture2DArray: %s"), *ArrayName);
		return false;
	}

	// 设置源贴图数组
	NewArray->SourceTextures = SourceTextures;
	NewArray->AddressX = TA_Clamp;
	NewArray->AddressY = TA_Clamp;
	NewArray->AddressZ = TA_Clamp;

	// 从源贴图构建数组数据
	if (!NewArray->UpdateSourceFromSourceTextures(true))
	{
		UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] UpdateSourceFromSourceTextures failed for %s"), *ArrayName);
		return false;
	}

	// 编译并更新资源
	NewArray->UpdateResource();
	OutArray = NewArray;

	UE_LOG(LogTemp, Log, TEXT("[TextureRegistry] Built %s: %dx%d, %d slices."),
		*ArrayName, SizeX, SizeY, SourceTextures.Num());
	return true;
}
#else
bool UVoxelTextureRegistry::BuildArrayFromTextures(
	const TArray<UTexture2D*>& SourceTextures,
	UObject* Outer,
	const FString& ArrayName,
	TObjectPtr<UTexture2DArray>& OutArray)
{
	return false;
}
#endif

int32 UVoxelTextureRegistry::GetTextureIndex(const FVoxelTextureSet& TextureSet) const
{
	const uint32 Hash = TextureSet.GetHash();
	if (const int32* Index = TextureSetToIndex.Find(Hash))
	{
		return *Index;
	}
	// 未注册的贴图集：报错（不静默 fallback）
	UE_LOG(LogTemp, Error, TEXT("[TextureRegistry] GetTextureIndex: Hash=%u NOT FOUND! BaseColor=%s Normal=%s ORM=%s"),
		Hash,
		TextureSet.BaseColorMap.IsValid() ? *TextureSet.BaseColorMap.ToSoftObjectPath().ToString() : TEXT("None"),
		TextureSet.NormalMap.IsValid() ? *TextureSet.NormalMap.ToSoftObjectPath().ToString() : TEXT("None"),
		TextureSet.ORMMap.IsValid() ? *TextureSet.ORMMap.ToSoftObjectPath().ToString() : TEXT("None"));
	return 0;
}
