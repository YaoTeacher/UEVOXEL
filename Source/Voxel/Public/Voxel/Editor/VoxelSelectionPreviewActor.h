// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelSelectionPreviewActor.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 选区预览 Actor（独立于 PreviewActor）
 *
 * 职责：
 * - 渲染 CurrentSelection 的体素高亮（SelectionMesh，程序化网格）
 * - 渲染选区包围盒外框（SelectionFrameMesh，静态网格）
 * - 跨工具通用：任何工具下只要有 CurrentSelection 就显示
 *
 * 设计原则：
 * - 独立于 PreviewActor 的工具预览生命周期
 * - 不参与 PreviewMode 切换
 * - 自身管理材质和网格生成
 */
UCLASS(Blueprintable, BlueprintType)
class VOXEL_API AVoxelSelectionPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelSelectionPreviewActor();

	// === AActor 接口 ===
	virtual void BeginPlay() override;

	// === 选区渲染接口 ===

	/**
	 * 更新选区高亮预览
	 * 用 BlockSystem.GenerateMeshData 生成网格（贪婪网格+纹理），写入 SelectionMesh。
	 * 同时更新 SelectionFrameMesh 显示选区包围盒外框。
	 *
	 * @param SelectedVoxels 选中的体素位置数组
	 * @param BlockLength 方块长度
	 */
	void UpdateSelectionPreview(const TArray<FIntVector>& SelectedVoxels, float BlockLength);

	/**
	 * 清除选区高亮预览（隐藏 SelectionMesh 和 SelectionFrameMesh）
	 */
	void ClearSelectionPreview();

	// === 组件访问器 ===

	/** 选区高亮网格组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Selection|Components")
	TObjectPtr<UProceduralMeshComponent> SelectionMesh;

	/** 选区外框组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Selection|Components")
	TObjectPtr<UStaticMeshComponent> SelectionFrameMesh;

	// === 材质配置 ===

	/** 选区高亮材质（网格预览材质，非 Chunk 材质） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection|Materials")
	TObjectPtr<UMaterialInterface> SelectionMeshMaterial;

	/** 外框框架材质（与 PreviewActor 共用同一材质类型） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection|Materials")
	TObjectPtr<UMaterialInterface> WireframeMaterial;

	/** 外框立方体 StaticMesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection|Meshes")
	TObjectPtr<UStaticMesh> BoxMesh;

protected:
	/** 选区动态材质实例（懒创建，带 BlockLength 参数） */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SelectionDynamicMaterial;

	/** 外框动态材质实例 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WireframeDynamicMaterial;
};
