// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/Enum/EditCommandEnums.h"
#include "Data/VoxelEditCommand.h"
#include "Data/CoreFragment.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Core/CoreTags.h"
#include "VoxelPreviewActor.generated.h"

// 前向声明
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UConfigDataSubsystem;
struct FChunkMeshData;
class AVoxel3DUIActor;
class AVoxelPlayerController;

UENUM()
enum class EPreviewMode : uint8
{
	None              = 0 UMETA(DisplayName = "None", Hidden),       // 无预览
	SingleVoxel       = 1 UMETA(DisplayName = "Single Voxel"),       // 单体素预览（方块网格）
	SimpleGeometry    = 2 UMETA(DisplayName = "Simple Geometry"),    // 简单几何体预览（外框+内部）
	ComplexBlueprint  = 3 UMETA(DisplayName = "Complex Blueprint"),  // 复杂蓝图预览（外框+程序化模型）
	BrushPreview      = 4 UMETA(DisplayName = "Brush Preview"),      // 笔刷预览（半透明高亮）
	Furniture         = 5 UMETA(DisplayName = "Furniture"),          // 家具预览（半透明包围盒+StaticMesh）
};

/**
 * 体素预览可视化 Actor（组件化设计）
 *
 * 设计原则：
 * - 一个玩家只有一个预览 Actor
 * - 根据当前操作动态切换显示内容
 * - 直接使用组件处理模型，不生成额外的 Actor
 * - 复杂蓝图预览接收 BlockSystem 计算的网格数据
 *
 * 组件架构：
 * - WireframeMesh (UStaticMeshComponent*) - 外框线框
 * - InnerMesh (UStaticMeshComponent*) - 内部几何体
 * - BlueprintMesh (UProceduralMeshComponent*) - 复杂蓝图（接收 FChunkMeshData）
 *
 * 数据流：
 * - BlockSystem 计算 FChunkMeshData → PreviewActor 渲染
 */
UCLASS(Blueprintable, BlueprintType)
class VOXEL_API AVoxelPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	// === 预览模式枚举 ===

	/**
	 * 预览模式类型
	 * 用于切换预览 Actor 的显示内容
	 */


	// === 构造函数 ===
	AVoxelPreviewActor();

	// === 核心功能 ===

	/**
	 * 设置预览模式
	 * @param Mode 预览模式
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void SetPreviewMode(EPreviewMode Mode);

	/**
	 * 获取当前预览模式
	 * @return 当前预览模式
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Preview")
	EPreviewMode GetPreviewMode() const { return CurrentMode; }

	/**
	 * 更新单体素预览（模式1）
	 * 复用 AVoxelUIBlockActor 的材质切换逻辑
	 *
	 * @param BlockIndex 体素索引（内部转换为中心世界坐标）
	 * @param BlockType 方块类型（用于获取实际材质）
	 * @param Color 预览颜色（当找不到材质时使用）
	 * @param Size 体素大小（默认 100cm）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateSingleVoxelPreview(FIntVector BlockIndex, int32 BlockType, FLinearColor Color, float Size = 100.0f);

	/**
	 * 更新简单几何体预览（模式2）- 不带选区数据
	 * 支持厚度、旋转、正方体模式
	 *
	 * @param Request 编辑请求
	 * @param BlockLength 方块长度
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateSimpleGeometryPreview(
		const FVoxelEditRequest& Request,
		float BlockLength
	);

	/**
	 * 更新简单几何体预览（模式2）- 带选区数据
	 * 选区工具使用此版本
	 *
	 * @param Request 编辑请求
	 * @param BlockLength 方块长度
	 * @param SelectedVoxels 实际选中的体素
	 * @param BlockType 选区体素渲染用的方块类型（决定 Chunk 材质与纹理索引），默认 1
	 */
	void UpdateSimpleGeometryPreviewWithSelection(
		const FVoxelEditRequest& Request,
		float BlockLength,
		const TArray<FIntVector>& SelectedVoxels,
		int32 BlockType = 1
	);

	// ==================== Mesh独立更新方法 ====================

	// ==================== Mesh独立更新方法 ====================

	/**
	 * 更新WireframeMesh（外框）
	 * @param Bounds 边界框
	 * @param GeometryCenter 旋转中心
	 * @param GeometryRotation 旋转角度
	 * @param BlockLength 方块长度（用于动态设置材质的 BlockLength 参数，<=0 时跳过）
	 */
	void UpdateWireframeMesh(
		const FBox& Bounds,
		const FVector& GeometryCenter,
		const FRotator& GeometryRotation,
		float BlockLength = 0.0f
	);

	/**
	 * 更新InnerMesh（内部几何体）
	 * 包含ThicknessMesh的更新逻辑
	 * @param Geometry 几何描述
	 * @param BaseBounds 基础边界
	 * @param GeometryCenter 旋转中心
	 * @param GeometryRotation 旋转角度
	 * @param BlockLength 方块长度
	 */
	void UpdateInnerMesh(
		const FVoxelGeometryDesc& Geometry,
		const FBox& BaseBounds,
		const FVector& GeometryCenter,
		const FRotator& GeometryRotation,
		float BlockLength
	);

	/**
	 * 设置 InnerMesh/ThicknessMesh 显隐
	 * 选区形状工具不需要 InnerMesh（选区由 SelectionMesh 渲染），只保留 WireframeMesh
	 * @param bVisible true=显示, false=隐藏
	 */
	void SetInnerMeshVisible(bool bVisible);

	/**
	 * 更新ProceduralMesh（程序化网格）
	 * 用于显示实际选中的体素（带面剔除）
	 * @param SelectedVoxels 选中的体素位置
	 * @param BlockLength 方块长度
	 * @param Origin 局部坐标原点（通常为 GeometryCenter）
	 * @param BlockType 方块类型（默认 1）
	 */
	void UpdateProceduralMesh(
		const TArray<FIntVector>& SelectedVoxels,
		float BlockLength,
		const FVector& Origin = FVector::ZeroVector,
		int32 BlockType = 1
	);

	/**
	 * 更新复杂蓝图预览（模式3）
	 * 外框使用立方体，内部接收 BlockSystem 生成的网格数据
	 *
	 * @param Bounds 预览区域边界框
	 * @param MeshData 网格数据（由 BlockSystem 计算）
	 * @param BlockType 方块类型
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateComplexBlueprintPreview(FBox Bounds, const FChunkMeshData& MeshData, int32 BlockType, float BlockLength = 0.0f);

	/**
	 * 更新模板预览（封装位置/旋转中心/padding 计算）
	 * 调用方只需提供模板数据和几何描述，本方法内部计算：
	 * - Origin（放置原点，XY 中心对齐鼠标点）
	 * - RotationCenter（XY 底面中心，旋转绕此点）
	 * - Bounds（世界 AABB）
	 * - MeshRelativeOffset（padding +1 补偿）
	 * - BlueprintMesh/WireframeMesh 相对位置/旋转
	 *
	 * @param Tmpl 模板数据（Entries 含体素相对坐标）
	 * @param Geometry 几何描述（Box.MinIndex=鼠标点，Rotation=旋转）
	 * @param MeshData 已生成的网格数据（由调用方通过 BuildTemplateContext + GenerateMeshData 生成）
	 * @param BlockType 方块类型（材质用）
	 * @param BlockLength 方块长度
	 */
	void UpdateTemplatePreview(
		const struct FVoxelTemplate& Tmpl,
		const FVoxelGeometryDesc& Geometry,
		const FChunkMeshData& MeshData,
		int32 BlockType,
		float BlockLength);

	/**
	 * 更新模板预览中的家具（多家具渲染）
	 * 遍历模板 Entries 中的 BuildItem 类型，为每个家具获取/创建 StaticMeshComponent 并渲染半透明预览。
	 * 必须在 UpdateTemplatePreview 之后调用（共享同一 Actor 位置/旋转）。
	 *
	 * @param Tmpl 模板数据
	 * @param Geometry 几何描述（Box.MinIndex=鼠标点，Rotation=旋转）
	 * @param Remap 素材替换表（原 FullId → 新 FullId）
	 * @param CDS 配置数据子系统（FullId → Handle → Item 查找）
	 * @param BlockLength 方块长度
	 */
	void UpdateTemplateFurniturePreview(
		const struct FVoxelTemplate& Tmpl,
		const FVoxelGeometryDesc& Geometry,
		const TMap<FString, FString>& Remap,
		UConfigDataSubsystem* CDS,
		float BlockLength);

	/**
	 * 更新笔刷预览（模式4）
	 * 在地形上显示半透明高亮的笔刷影响范围
	 * 复用 UpdateProceduralMesh 的网格生成逻辑，但用 BrushPreviewMaterial 渲染
	 *
	 * @param CenterIndex 笔刷中心体素索引
	 * @param BrushSize 笔刷尺寸
	 * @param Shape 笔刷形状（Box/Sphere/Cylinder）
	 * @param Dimension 笔刷维度（3D/2D）
	 * @param BlockLength 方块长度
	 * @param Color 高亮颜色（默认绿色）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateBrushPreview(
		const FIntVector& CenterIndex,
		int32 BrushSize,
		EBatchShapeType Shape,
		EBrushDimension Dimension,
		float BlockLength,
		FLinearColor Color = FLinearColor::Green
	);

	/**
	 * 更新家具预览（模式5）
	 * 渲染半透明包围盒（WireframeMesh）+ 家具 StaticMesh 半透明预览（FurniturePreviewMesh）
	 *
	 * @param PlaceIndex 放置体素索引（家具原点）
	 * @param Direction 朝向
	 * @param FurnitureMesh 家具的静态网格（来自 AssetFragment.Mesh）
	 * @param Bounds 体素坐标的包围盒（来自 GetFurnitureBounds）
	 * @param BlockLength 方块长度
	 * @param TextureSet 家具的 PBR 贴图集（来自 AssetFragment.TextureSet，用于设置 MID 的 BaseColorMap/NormalMap/ORMMap）
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateFurniturePreview(
		const FIntVector& PlaceIndex,
		EActorDirection Direction,
		UStaticMesh* FurnitureMesh,
		const FBox& Bounds,
		float BlockLength,
		const FVoxelTextureSet& TextureSet
	);

	/**
	 * 清空家具预览池（隐藏所有池中组件，重置变换）
	 * 由 ClearPreview 和 UpdateTemplateFurniturePreview 调用
	 */
	void ClearFurniturePreviewPool();

	/**
	 * 获取/创建家具预览池中的组件（按需扩展池大小）
	 * @param Index 池中的索引（0=第一个家具，1=第二个...）
	 * @return 可用的 UStaticMeshComponent（已注册，初始为隐藏状态）
	 */
	UStaticMeshComponent* AcquireFurnitureMeshComponent(int32 Index);

	/**
	 * 清除预览可视化
	 * 隐藏所有组件
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void ClearPreview();

	/**
	 * 设置预览有效性（切换正常/红色无效材质）
	 * @param bValid true=正常半透明预览, false=红色无效预览
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void SetPreviewValid(bool bValid);

	/**
	 * 清除笔刷预览（仅清理 BrushPreviewMesh，不影响选区）
	 * 用于工具切换、鼠标离开等场景
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void ClearBrushPreview();

	/**
	 * 设置预览可见性
	 * @param bVisible 是否可见
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void SetPreviewVisible(bool bVisible);

	/**
	 * 移动预览
	 * @param DeltaPosition 位置偏移
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void MovePreview(FVector DeltaPosition);

	/**
	 * 旋转预览
	 * @param Rotation 旋转量
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void RotatePreview(FRotator Rotation);
	
	// ==================== 预览交互状态管理 ====================

	/**
	 * 设置预览交互状态
	 * @param NewState 新的交互状态
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void SetPreviewState(EPreviewInteractionState NewState);

	/**
	 * 【已废弃】获取当前预览交互状态
	 * @return 始终返回 HIDDEN（状态已迁移到 StateComponent）
	 * @note 保留以兼容旧蓝图调用，请改用 StateComponent->GetBuildPhase()
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Preview", meta = (DeprecatedFunction, DeprecationMessage = "PreviewState 已迁移到 StateComponent，请使用 GetBuildPhase()"))
	EPreviewInteractionState GetPreviewState() const { return EPreviewInteractionState::IDLE; }

	/**
	 * 检查是否可以转换到指定状态
	 * @param NewState 目标状态
	 * @return true 如果可以转换，false 否则
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Preview")
	bool CanTransitionTo(EPreviewInteractionState NewState) const;

	// ==================== 统一更新接口 ====================

	/**
	 * 从编辑请求统一更新所有预览子项（渲染+Gizmo）
	 *
	 * @param Request 编辑请求（包含 Geometry + BuildState + EditState + BlockType）
	 * @param BlockLength
	 * @note PreviewActor会根据Request自动决定如何渲染和显示Gizmo
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
	void UpdateFromEditRequest(const struct FVoxelEditRequest& Request);

	// ==================== 工具基类接口 ====================

	/**
	 * 设置预览的体素位置（用于可视化受影响的体素）
	 * @param VoxelPositions 体素位置数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview|Tools")
	void SetVoxelPositions(const TArray<FIntVector>& VoxelPositions);

	/**
	 * 清除预览显示
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Preview|Tools")
	void ClearPreviewDisplay();

protected:
	// === AActor 接口 ===
	virtual void BeginPlay() override;

public:
	// === 核心组件（组件化设计） ===

	/**
	 * 外框网格组件（用于简单几何体和复杂蓝图的外框线框）
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
	TObjectPtr<UStaticMeshComponent> WireframeMesh;

	/**
	 * 内部网格组件（用于单体素和简单几何体的内部填充）
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
	TObjectPtr<UStaticMeshComponent> InnerMesh;

	/**
	 * 厚度网格组件（用于显示挖空或扩张边界）
	 * Thickness > 0: 显示内部挖空边界
	 * Thickness < 0: 显示外部扩张边界
	 * Thickness = 0: 隐藏
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
	TObjectPtr<UStaticMeshComponent> ThicknessMesh;

	/**
	 * 复杂蓝图组件（程序化生成）
	 * 用于显示复杂蓝图的实际结构
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
	TObjectPtr<UProceduralMeshComponent> BlueprintMesh;

	/**
	 * 笔刷预览组件（独立程序化网格）
	 * 用于 hover 时显示笔刷影响范围，与选区渲染（BlueprintMesh）完全独立，互不干扰
	 * - 选区：WireframeMesh（外框）+ BlueprintMesh section 0（体素高亮）
	 * - 笔刷预览：BrushPreviewMesh（本组件）
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
	TObjectPtr<UProceduralMeshComponent> BrushPreviewMesh;

	/**
	 * 家具预览组件池（动态管理，按需创建）
	 * - 单家具预览：使用池中 Index=0 的组件
	 * - 模板预览：遍历模板家具，为每个家具获取/创建一个组件
	 * 池会保留已创建的组件避免每帧创建/销毁，由 ClearFurniturePreviewPool 统一隐藏
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FurniturePreviewMeshPool;

	/** 当前家具预览池中活跃的组件数量（Index < 此值的组件为可见状态） */
	int32 FurniturePreviewActiveCount = 0;

	// === 材质配置 ===

	/** 外框框架材质（父材质，运行时基于此创建 WireframeDynamicMaterial）
	 *  用于框架线框效果，不要与体素网格材质（VoxelGridMaterial）混用
	 *  若留空则使用 WireframeMesh 的默认材质 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> WireframeMaterial;

	/** 体素网格材质（父材质，运行时基于此创建 VoxelGridDynamicMaterial）
	 *  推荐配置为 M_VoxelGrid 或 MI_VoxelGrid_Inst 以实现体素格线框效果
	 *  若配置则在选区模式下覆盖 WireframeMesh 的框架材质，显示基于体素大小的网格线 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> VoxelGridMaterial;

	/** 笔刷预览材质（父材质，运行时基于此创建 BrushPreviewDynamicMaterial）
	 *  推荐配置为 M_BrushPreview 以实现半透明高亮效果
	 *  用于编辑笔刷和选区笔刷在地形上的形状预览 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> BrushPreviewMaterial;

	/** 模板预览材质（半透明 Chunk 材质）
	 *  用于 BlueprintMesh 显示模板放置预览。应配置为半透明版本的 Chunk 图集材质。
	 *  若留空则回退到当前 Chunk 材质。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> TemplatePreviewMaterial;

	/** 家具预览材质（半透明）
	 *  用于 FurniturePreviewMesh 显示家具 StaticMesh 的半透明预览。
	 *  推荐配置为 M_FurniturePreview（半透明、不受光照）。
	 *  若留空则用引擎默认半透明材质。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> FurniturePreviewMaterial;

	/** 家具预览无效材质（红色半透明）
	 *  当放置位置无效时使用此材质显示红色提示。
	 *  推荐配置为 M_FurniturePreviewInvalid。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> FurniturePreviewInvalidMaterial;

	/** 模板预览无效材质（红色半透明）
	 *  当模板放置位置无效时使用此材质显示红色提示。
	 *  推荐配置为 M_TemplatePreviewInvalid。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Materials")
	TObjectPtr<UMaterialInterface> TemplatePreviewInvalidMaterial;

	// === StaticMesh 资产配置 ===

	/** 方块 StaticMesh（用于单体素和立方体外框） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Meshes")
	TObjectPtr<UStaticMesh> BoxMesh;

	/** 球体 StaticMesh（用于球体预览） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Meshes")
	TObjectPtr<UStaticMesh> SphereMesh;

	/** 圆柱体 StaticMesh（用于圆柱预览） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Meshes")
	TObjectPtr<UStaticMesh> CylinderMesh;

	/** 线框立方体 StaticMesh（用于外框） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Meshes")
	TObjectPtr<UStaticMesh> WireframeBoxMesh;

private:
	/** 当前预览模式 */
	EPreviewMode CurrentMode;

	/** 所属 Controller（弱引用，避免循环引用） */
	UPROPERTY()
	TWeakObjectPtr<AVoxelPlayerController> OwningController;

	/** 动态材质实例（外框框架） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WireframeDynamicMaterial;

	/** 动态材质实例（体素网格） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> VoxelGridDynamicMaterial;

	/** 动态材质实例（笔刷预览） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BrushPreviewDynamicMaterial;

	/** 动态材质实例（模板预览，基于 TemplatePreviewMaterial 或 Chunk 材质） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TemplatePreviewDynamicMaterial;

	/** 动态材质实例（家具预览，基于 FurniturePreviewMaterial 或默认半透明） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FurniturePreviewDynamicMaterial;

	/** 动态材质实例（内部） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> InnerDynamicMaterial;

	/** 动态材质实例（厚度） */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ThicknessDynamicMaterial;

	/** 当前预览框（世界坐标） */
	FBox CurrentBounds = FBox();

	/** 当前预览是否有效（可放置）*/
	bool bPreviewValid = true;
	/**
	 * 更新组件可见性（根据当前模式）
	 */
	void UpdatePreviewComponentVisibility();

	/**
	 * 根据形状类型更新网格（复现 UpdateAreaByBox 逻辑）
	 * @param MeshComponent 要更新的网格组件
	 * @param ShapeType 形状类型
	 * @param Bounds 边界框
	 * @param GeometryCenter 旋转中心点
	 * @param GeometryRotation 几何旋转
	 */
	void UpdateMeshByShape(UStaticMeshComponent* MeshComponent, EBatchShapeType ShapeType, FBox Bounds, FVector GeometryCenter, FRotator GeometryRotation);

	/**
	 * 根据方块类型获取材质
	 * @param BlockType 方块类型标签
	 * @return 方块材质，如果找不到则返回 nullptr
	 */
	UMaterialInterface* GetMaterialByBlockType(int32 BlockType);

	/**
	 * 为 MID 设置 Texture2DArray 参数（BaseColorArray/NormalArray/ORMArray）
	 * 从 VoxelTextureRegistry 获取运行时生成的贴图数组
	 * @param MID 要设置参数的动态材质实例
	 */
	void ApplyTextureArrayParams(UMaterialInstanceDynamic* MID);

	/**
	 * 添加立方体网格数据（用于ProceduralMesh）
	 * @param OutMeshData 输出网格数据
	 * @param BoxMin 包围盒最小点
	 * @param BoxMax 包围盒最大点
	 */
	void AddCubeMeshData(
		FChunkMeshData& OutMeshData,
		const FVector& BoxMin,
		const FVector& BoxMax
	);

	/**
	 * 添加单个面网格数据（用于ProceduralMesh，带面剔除）
	 * @param OutMeshData 输出网格数据
	 * @param BoxMin 包围盒最小点
	 * @param BoxMax 包围盒最大点
	 * @param FaceIndex 面索引（0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z）
	 * @param Color 顶点颜色（alpha=纹理索引）
	 */
	void AddFaceMeshData(
		FChunkMeshData& OutMeshData,
		const FVector& BoxMin,
		const FVector& BoxMax,
		int32 FaceIndex,
		const FColor& Color
	);

	/**
	 * 更新笔刷预览网格（独立组件，不影响选区 BlueprintMesh）
	 * 复用面剔除逻辑，但写入 BrushPreviewMesh 的 section 0
	 * @param BrushVoxels 笔刷影响的体素索引数组
	 * @param BlockLength 方块长度
	 * @param Origin 局部坐标原点（通常为笔刷范围最小点对应的世界坐标）
	 */
	void UpdateBrushPreviewMesh(
		const TArray<FIntVector>& BrushVoxels,
		float BlockLength,
		const FVector& Origin
	);

	// ==================== 状态处理方法 ====================

	/**
	 * 获取方块长度（从 BlockWorldSubsystem）
	 * @return 方块长度（单位：米）
	 */
	float GetBlockLength() const;

	// ==================== 统一更新接口内部实现 ====================
	
	/**
	 * 根据编辑命令获取预览颜色
	 * @param EditCommand 编辑命令
	 * @return 预览颜色
	 */
	FLinearColor GetPreviewColor(const FVoxelEditCommand& EditCommand) const;
};
