#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelCutoutBoxActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 透明区域框边框可视化 Actor
 *
 * 双模式 × 生效态：
 * - 编辑模式（Cutout 工具激活）：FillBox 浅白半透明填充 + 碰撞(Cutout通道)供选中编辑；WireframeBox 显示
 * - 其他工具：仅 WireframeBox 白色线框，无填充无碰撞
 * - 生效框：线框白/填充亮；失效框：线框灰/填充暗（整行 Opacity 降低由材质/颜色表达）
 *
 * 资产（BoxMesh/WireframeMaterial/FillMaterial）在 BP 配置；未配置时组件以网格默认材质渲染（守空指针）。
 */
UCLASS(Blueprintable)
class AVoxelCutoutBoxActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelCutoutBoxActor();

	/** 设置边框对应的框 Id（供工具射线选中后回查） */
	void SetBoxId(int32 InBoxId) { BoxId = InBoxId; }
	int32 GetBoxId() const { return BoxId; }

	/** 更新边框变换（体素闭区间 Min..Max） */
	void UpdateBounds(const FIntVector& MinIndex, const FIntVector& MaxIndex, float BlockLength);

	/** 设置生效/失效（着色：生效=白/亮，失效=灰/暗） */
	void SetEnabled(bool bInEnabled);

	/** 设置编辑模式（FillBox+碰撞显隐） */
	void SetEditMode(bool bInEditMode);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cutout")
	TObjectPtr<UStaticMeshComponent> WireframeBox;

	UPROPERTY(VisibleAnywhere, Category = "Cutout")
	TObjectPtr<UStaticMeshComponent> FillBox;

	/** 盒体网格（BP 配置，默认引擎 Cube） */
	UPROPERTY(EditDefaultsOnly, Category = "Cutout|Assets")
	TObjectPtr<UStaticMesh> BoxMesh;

	/** 线框材质（BP 配置） */
	UPROPERTY(EditDefaultsOnly, Category = "Cutout|Assets")
	TObjectPtr<UMaterialInterface> WireframeMaterial;

	/** 填充半透明材质（BP 配置，含 Color 向量参数） */
	UPROPERTY(EditDefaultsOnly, Category = "Cutout|Assets")
	TObjectPtr<UMaterialInterface> FillMaterial;

private:
	UMaterialInstanceDynamic* GetOrCreateWireframeMID();
	UMaterialInstanceDynamic* GetOrCreateFillMID();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WireframeMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	bool bEnabled = true;
	bool bEditMode = false;
	int32 BoxId = INDEX_NONE;
};
