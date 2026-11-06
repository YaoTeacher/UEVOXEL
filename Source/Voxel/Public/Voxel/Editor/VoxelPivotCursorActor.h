#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelPivotCursorActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * 地面标记 Actor
 *
 * 职责：
 * - 视野中心标记（PointMarker/CursorMesh）：跟随 Pawn 位置（视觉中心/屏幕中心），不再跟鼠标
 * - 浮空时显示落地竖线（DropLine）：从 Pawn 向下到最近地面点，展示位置与高度
 * - 按住鼠标中键时显示旋转环标记（RotationRings），指示可旋转状态
 *
 * 由 UVoxelPreviewComponent 创建和管理
 */
UCLASS()
class VOXEL_API AVoxelPivotCursorActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelPivotCursorActor();

	/** 设置标记世界位置（白球跟随 Pawn） */
	void SetCursorWorldLocation(const FVector& WorldLocation);

	/**
	 * 设置落地竖线：从 TopWorld 向下到 BottomWorld。
	 * 距离<=0 或重合时隐藏竖线（贴地时）。
	 */
	void SetDropLine(const FVector& TopWorld, const FVector& BottomWorld);

	/**
	 * 设置旋转环可见性
	 * @param bVisible true=显示旋转环（MMB 按住时），false=仅显示地面点
	 */
	void SetRotationRingsVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;

	/** 碰撞球（根组件，用于 UI 通道射线检测） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> CollisionSphere;

	/** 视野中心标记（白球，跟随 Pawn） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> CursorMesh;

	/** 落地竖线（Pawn 浮空时显示，从 Pawn 向下到地面） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> DropLine;

	/** 旋转环标记（仅 MMB 按住时显示） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> RotationRings;
};
