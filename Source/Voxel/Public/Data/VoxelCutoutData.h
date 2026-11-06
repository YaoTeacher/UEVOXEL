#pragma once

#include "CoreMinimal.h"
#include "VoxelCutoutData.generated.h"

/**
 * 透明区域框数据（Cutout Box）
 *
 * 框内体素判定为 Ghost（透明+穿射线+挡Pawn），用于穿过墙/屋顶查看与编辑内部。
 * - MinIndex/MaxIndex：体素索引闭区间（Clamp 到 WorldBorder）
 * - bEnabled：生效/失效（仅生效框参与地形 Ghost；失效框数据与边框保留）
 * - 持久化于 FSceneData.CutoutBoxes；增/删/改几何纳入撤销重做
 */
USTRUCT(BlueprintType)
struct FCutoutBoxData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	int32 Id = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	FIntVector MinIndex = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	FIntVector MaxIndex = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	bool bEnabled = true;

	/** 归属玩家（AVoxelPlayerController::PlayerId）；各客户端只渲染/生效本地玩家的框 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	int32 OwnerPlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Cutout")
	FName Label;

	FCutoutBoxData() = default;

	FCutoutBoxData(int32 InId, const FIntVector& InMin, const FIntVector& InMax, bool bInEnabled = true, FName InLabel = NAME_None, int32 InOwnerPlayerId = INDEX_NONE)
		: Id(InId), MinIndex(InMin), MaxIndex(InMax), bEnabled(bInEnabled), OwnerPlayerId(InOwnerPlayerId), Label(InLabel)
	{
	}

	/** 是否有效（Id 合法） */
	bool IsValidBox() const { return Id != INDEX_NONE; }

	/** 索引空间闭盒 FBox（Min..Max，含端点，与 BlockSys 切面盒同约定） */
	FBox ToClosedBox() const
	{
		return FBox(FVector(MinIndex), FVector(MaxIndex));
	}

	bool operator==(const FCutoutBoxData& Other) const
	{
		return Id == Other.Id;
	}
};

/** TMap 键哈希（按 Id） */
inline uint32 GetTypeHash(const FCutoutBoxData& Box)
{
	return GetTypeHash(Box.Id);
}
