#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/Item/VoxelItemBase.h"
#include "UVoxelTemplateUtils.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UConfigDataSubsystem;

UCLASS()
class VOXEL_API UVoxelTemplateUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ===== 放置 =====
	// Remap: 素材替换表（原 FullId → 新 FullId），空表示不替换
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static bool PlaceTemplate(
		UVoxelBlockWorldSubsystem* BlockSys,
		UVoxelItemWorldSubsystem* ItemWorld,
		UConfigDataSubsystem* ConfigSys,
		const FVoxelTemplate& Template,
		FIntVector Origin,
		EActorDirection Direction,
		const TMap<FString, FString>& Remap);

	// ===== 素材替换辅助 =====
	// 统计模板内去重的可替换素材的 FullId（仅 Block / BuildItem 类型），保持稳定顺序
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static TArray<FString> GetDistinctMaterials(const FVoxelTemplate& Template, UConfigDataSubsystem* ConfigSys);

	// 计算模板以 CenterIndex 为 XY 中心时的实际放置原点（最小角/局部原点）
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static FIntVector GetCenteredPlacementOrigin(const FVoxelTemplate& Template, FIntVector CenterIndex);

	// 计算某素材的"同类"过滤 Tag：方块→Item.Type.Block（任意方块互换）；
	// 家具→其在 Item.Type.BuildItem 下的直接子分类（如 Chair 只换 Chair）
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static FGameplayTag GetRemapFilterTag(const FString& FullId, UConfigDataSubsystem* ConfigSys);

	// ===== 从选区捕获 =====
	static FVoxelTemplate CaptureFromSelection(
		UVoxelBlockWorldSubsystem* BlockSys,
		UVoxelItemWorldSubsystem* ItemWorld,
		UConfigDataSubsystem* ConfigSys,
		const TArray<FIntVector>& Selection,
		const FTemplateMeta& Meta);

	// ===== 旋转辅助 =====
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static FIntVector RotateOffset(FIntVector Offset, EActorDirection Direction);

	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static EActorDirection ComposeDirection(EActorDirection Base, EActorDirection Apply);

	// ===== 过滤 =====
	UFUNCTION(BlueprintCallable, Category = "Voxel|Template")
	static bool ShouldSkipFromTemplate(int32 ItemId, UConfigDataSubsystem* ConfigSys);

	// ===== 包围盒计算 =====
	static FBox ComputeSelectionBounds(const TArray<FIntVector>& Selection);

	// ===== .tmpl 持久化 =====
	static bool SaveToTmplFile(const FVoxelTemplate& T, const FString& FilePath);
	static bool LoadFromTmplFile(const FString& FilePath, FVoxelTemplate& OutT);

	// ===== .vtpl 导出导入 =====
	static bool ExportToVtpl(const FVoxelTemplate& T, const FString& VtplPath, UConfigDataSubsystem* ConfigSys);
	static bool ImportFromVtpl(const FString& VtplPath, UConfigDataSubsystem* ConfigSys, FVoxelTemplate& OutT);

	// ===== 模板默认存储路径 =====
	static FString GetTemplateDir() { return FPaths::ProjectSavedDir() / TEXT("Templates"); }
	static FString GetTmplFilePath(const FGuid& Guid) { return GetTemplateDir() / (Guid.ToString() + TEXT(".tmpl")); }

	// ===== 生成预览网格 =====
	static bool GenerateTemplatePreviewMesh(
		const FVoxelTemplate& T,
		UConfigDataSubsystem* ConfigSys,
		float BlockLength,
		struct FChunkMeshData& OutMeshData);
};
