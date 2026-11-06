# Voxel 编辑器系�?- 第一阶段实现计划

> **For Claude:** 必需子技能：使用 superpowers:executing-plans 逐步执行此计划�?

**目标�?* �?UE5 体素沙盒游戏构建游戏内体素编辑系统，支持快速批量操作、预览系统、历史管理和可扩展的工具架构�?

**架构�?*
- 中央协调器：`UVoxelEditorModeSubsystem` (UWorldSubsystem) 管理所有编辑器子系�?
- 通过委托进行事件驱动的松耦合通信
- 基于区块的批量处理，集成现有�?`VoxelBlockWorldSubsystem`
- 独立预览 Actor (AVoxelPreviewActor) 共享网格生成算法
- 基于 GameplayTag 的工具系统实现可扩展�?
- 3D Gizmos + 2D 控制面板混合系统

**技术栈�?*
- UE5.7 C++
- ProceduralMeshComponent 用于预览渲染
- GameplayTag 系统用于工具分类
- 命令模式用于撤销/重做
- UE5 SaveGame 系统用于持久�?
- Slate/UMG 用于 UI 组件

---

## 第一阶段概述：核心基础

本阶段实现编辑器的基础子系统、数据结构和基础的单体素编辑功能�?

### 要构建的组件�?
1. **数据结构** - `FVoxelEditRequest`, `FEditPreviewInfo`
2. **UVoxelEditorModeSubsystem** - 中央协调�?
3. **UVoxelBatchEditManager** - 批量编辑处理�?
4. **UVoxelEditHistoryManager** - 撤销/重做系统
5. **AVoxelPreviewActor** - 独立预览可视�?
6. **基础单体素编�?* - 批量操作的基础

### 成功标准�?
- 可以进入/退出编辑器模式
- 可以放置/删除单体�?
- 可以撤销/重做单次编辑
- 预览显示放置位置
- 集成自动保存

---

## 任务 1：定义核心数据结�?

**文件�?*
- 创建：`Source/Voxel/Private/Data/Editor/VoxelEditorData.h`
- 创建：`Source/Voxel/Private/Data/Editor/VoxelEditorData.cpp`

**步骤 1：创建头文件，定义核心结构体**

```cpp
// VoxelEditorData.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/ControllerComponent/VoxelControllerStateComponent.h"
#include "VoxelEditorData.generated.h"

UENUM(BlueprintType)
enum class EBatchShapeType : uint8
{
    Single,     // 单个体素
    Box,        // 盒子
    Sphere,     // 球体
    Cylinder    // 圆柱�?
};

UENUM(BlueprintType)
enum class EEditConfirmType : uint8
{
    Immediate,      // 立即执行（单个体素）
    RequireConfirm  // 需要确认（批量编辑�?
};

/** 编辑请求描述�?- 数学描述要编辑的内容 */
USTRUCT(BlueprintType)
struct FVoxelEditRequest
{
    GENERATED_BODY()

    /** 形状类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    EBatchShapeType ShapeType = EBatchShapeType::Single;

    /** 要放置的方块类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    FGameplayTag NewBlockType;

    /** 编辑模式（添�?替换/删除�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    EVoxelControllerEditState EditMode = EVoxelControllerEditState::Add;

    /** 尺寸（用�?Box/Sphere/Cylinder�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    FVector Dimensions = FVector::ZeroVector;

    /** 空心形状的厚�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    int32 Thickness = 0;

    /** 世界空间包围�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    FBox WorldBounds;

    /** 用于旋转/缩放的变�?*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Edit|Request")
    FTransform Transform;

    /** 从参数初始化请求 */
    UFUNCTION(BlueprintCallable, Category = "Edit|Request")
    void Init(const FTransform& InTransform, EBatchShapeType InShape,
              FVector InDimensions, FGameplayTag InBlock,
              EVoxelControllerEditState InMode, int32 InThickness = 0);

    /** 检查世界位置是否在此编辑形状内 */
    UFUNCTION(BlueprintCallable, Category = "Edit|Request")
    bool IsPointInside(const FVector& WorldPosition, float BlockSize) const;

private:
    FMatrix WorldToLocal;
    bool bIsInitialized = false;

    void BuildWorldToLocalMatrix();
};

/** 用于 UI 的预览信�?*/
USTRUCT(BlueprintType)
struct FEditPreviewInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 AffectedVoxelCount = 0;

    UPROPERTY(BlueprintReadOnly)
    EVoxelControllerEditState EditMode = EVoxelControllerEditState::Add;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag BlockType;

    UPROPERTY(BlueprintReadOnly)
    FBox AffectedBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
};

/** 用于撤销/重做的编辑命�?*/
USTRUCT(BlueprintType)
struct FVoxelEditCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FIntVector, FGameplayTag> ModifiedVoxels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;
};
```

**步骤 2：创建实现文�?*

```cpp
// VoxelEditorData.cpp
#include "VoxelEditorData.h"
#include "Kismet/KismetMathLibrary.h"

void FVoxelEditRequest::Init(const FTransform& InTransform, EBatchShapeType InShape,
                             FVector InDimensions, FGameplayTag InBlock,
                             EVoxelControllerEditState InMode, int32 InThickness)
{
    Transform = InTransform;
    ShapeType = InShape;
    Dimensions = InDimensions;
    NewBlockType = InBlock;
    EditMode = InMode;
    Thickness = InThickness;

    // 根据形状计算世界空间包围�?
    FVector Origin = Transform.GetLocation();
    switch (ShapeType)
    {
        case EBatchShapeType::Single:
            WorldBounds = FBox(Origin, Origin + FVector(1.0f));
            break;

        case EBatchShapeType::Box:
            WorldBounds = FBox::BuildAABB(Origin, Dimensions);
            break;

        case EBatchShapeType::Sphere:
        {
            float Radius = Dimensions.X;
            WorldBounds = FBox::BuildAABB(Origin - FVector(Radius), FVector(Radius * 2));
            break;
        }

        case EBatchShapeType::Cylinder:
        {
            float Radius = Dimensions.X;
            float Height = Dimensions.Z;
            FVector Min = Origin - FVector(Radius, Radius, 0);
            FVector Max = Origin + FVector(Radius, Radius, Height);
            WorldBounds = FBox(Min, Max);
            break;
        }
    }

    BuildWorldToLocalMatrix();
    bIsInitialized = true;
}

void FVoxelEditRequest::BuildWorldToLocalMatrix()
{
    WorldToLocal = Transform.ToMatrixWithScale().Inverse();
}

bool FVoxelEditRequest::IsPointInside(const FVector& WorldPosition, float BlockSize) const
{
    if (!bIsInitialized) return false;

    // 变换到局部空�?
    FVector LocalPos = WorldToLocal.TransformPosition(WorldPosition);

    switch (ShapeType)
    {
        case EBatchShapeType::Single:
            return FVector::Dist(LocalPos, FVector::ZeroVector) < BlockSize;

        case EBatchShapeType::Box:
            return FMath::Abs(LocalPos.X) <= Dimensions.X * 0.5f &&
                   FMath::Abs(LocalPos.Y) <= Dimensions.Y * 0.5f &&
                   FMath::Abs(LocalPos.Z) <= Dimensions.Z * 0.5f;

        case EBatchShapeType::Sphere:
            return LocalPos.Size() <= Dimensions.X;

        case EBatchShapeType::Cylinder:
        {
            float Radius = Dimensions.X;
            float Height = Dimensions.Z;
            return FMath::Sqrt(LocalPos.X * LocalPos.X + LocalPos.Y * LocalPos.Y) <= Radius &&
                   LocalPos.Z >= 0 && LocalPos.Z <= Height;
        }

        default:
            return false;
    }
}
```

**步骤 3：更�?Voxel.Build.cs 确保模块依赖**

检查：`Source/Voxel/Voxel.Build.cs`
验证包含�?
- "Core"
- "CoreUObject"
- "Engine"
- "InputCore"
- "EnhancedInput"
- "NavigationSystem"
- "AIModule"
- "ProceduralMeshComponent"
- "DataRegistry"
- "GameplayTags"

**步骤 4：提�?*

```bash
git add Source/Voxel/Private/Data/Editor/VoxelEditorData.h
git add Source/Voxel/Private/Data/Editor/VoxelEditorData.cpp
git commit -m "feat(editor): 添加核心编辑器数据结�?

- 添加 FVoxelEditRequest 用于编辑操作描述
- 添加 FEditPreviewInfo 用于预览信息
- 添加 FVoxelEditCommand 用于撤销/重做命令
- 实现形状检测逻辑（Box/Sphere/Cylinder/Single�?
- 支持旋转编辑的坐标变�?
```

---

## 任务 2：创�?VoxelEditorModeSubsystem（中央协调器�?

**文件�?*
- 创建：`Source/Voxel/Public/Voxel/Editor/VoxelEditorModeSubsystem.h`
- 创建：`Source/Voxel/Private/Voxel/Editor/VoxelEditorModeSubsystem.cpp`

**步骤 1：创建头文件**

```cpp
// VoxelEditorModeSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/SubsystemCollection.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelEditorModeSubsystem.generated.h"

// 前向声明
class UVoxelCameraManager;
class UVoxelBatchEditManager;
class UVoxelEditHistoryManager;
class UVoxelEditorToolManager;
class UVoxelGizmoManager;
class UVoxelClipboardManager;
class UVoxelBlueprintManager;
class AVoxelAreaBaseInstance;

UENUM(BlueprintType)
enum class EEditorModeState : uint8
{
    Inactive,  // 未激�?
    Entering,  // 进入�?
    Active,    // 已激�?
    Exiting    // 退出中
};

/**
 * 所有体素编辑子系统的中央协调器
 * 管理编辑器生命周期和子系统通信
 */
UCLASS(Blueprintable, BlueprintType)
class VOXEL_API UVoxelEditorModeSubsystem : public UWorldSubsystem, public FTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // === 子系统接�?===
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return true; }
    virtual TStatId GetStatId() const override;

    // === 编辑器模式控�?===

    /** 进入指定区域的编辑器模式 */
    UFUNCTION(BlueprintCallable, Category = "Editor|Mode")
    void EnterEditorMode(AVoxelAreaBaseInstance* TargetArea);

    /** 退出编辑器模式 */
    UFUNCTION(BlueprintCallable, Category = "Editor|Mode")
    void ExitEditorMode();

    /** 检查当前是否在编辑器模�?*/
    UFUNCTION(BlueprintPure, Category = "Editor|State")
    bool IsInEditorMode() const { return CurrentState == EEditorModeState::Active; }

    /** 获取当前编辑器状�?*/
    UFUNCTION(BlueprintPure, Category = "Editor|State")
    EEditorModeState GetCurrentState() const { return CurrentState; }

    /** 获取当前编辑区域 */
    UFUNCTION(BlueprintPure, Category = "Editor|State")
    AVoxelAreaBaseInstance* GetCurrentArea() const { return CurrentEditingArea; }

    // === 子系统访�?===

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelCameraManager* GetCameraManager() const { return CameraManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelBatchEditManager* GetBatchEditManager() const { return BatchEditManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelEditHistoryManager* GetHistoryManager() const { return HistoryManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelEditorToolManager* GetToolManager() const { return ToolManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelGizmoManager* GetGizmoManager() const { return GizmoManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelClipboardManager* GetClipboardManager() const { return ClipboardManager; }

    UFUNCTION(BlueprintPure, Category = "Editor|Managers")
    UVoxelBlueprintManager* GetBlueprintManager() const { return BlueprintManager; }

    // === 事件 ===

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditorModeEntered, AVoxelAreaBaseInstance*, TargetArea);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEditorModeExited);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditorStateChanged, EEditorModeState, NewState);

    /** 进入编辑器模式时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnEditorModeEntered OnEditorModeEntered;

    /** 退出编辑器模式时广�?*/
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnEditorModeExited OnEditorModeExited;

    /** 编辑器状态改变时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnEditorStateChanged OnEditorStateChanged;

protected:
    // === 子系统引�?===

    UPROPERTY()
    TObjectPtr<UVoxelCameraManager> CameraManager;

    UPROPERTY()
    TObjectPtr<UVoxelBatchEditManager> BatchEditManager;

    UPROPERTY()
    TObjectPtr<UVoxelEditHistoryManager> HistoryManager;

    UPROPERTY()
    TObjectPtr<UVoxelEditorToolManager> ToolManager;

    UPROPERTY()
    TObjectPtr<UVoxelGizmoManager> GizmoManager;

    UPROPERTY()
    TObjectPtr<UVoxelClipboardManager> ClipboardManager;

    UPROPERTY()
    TObjectPtr<UVoxelBlueprintManager> BlueprintManager;

private:
    UPROPERTY()
    EEditorModeState CurrentState = EEditorModeState::Inactive;

    UPROPERTY()
    AVoxelAreaBaseInstance* CurrentEditingArea = nullptr;

    void InitializeSubsystems();
    void ShutdownSubsystems();
    void SetEditorState(EEditorModeState NewState);
};
```

**步骤 2：创建实现文�?*

```cpp
// VoxelEditorModeSubsystem.cpp
#include "Voxel/Editor/VoxelEditorModeSubsystem.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Voxel/Editor/Managers/VoxelCameraManager.h"
#include "Voxel/Editor/Managers/VoxelBatchEditManager.h"
#include "Voxel/Editor/Managers/VoxelEditHistoryManager.h"
#include "Voxel/Editor/Managers/VoxelEditorToolManager.h"
#include "Voxel/Editor/Managers/VoxelGizmoManager.h"
#include "Voxel/Editor/Managers/VoxelClipboardManager.h"
#include "Voxel/Editor/Managers/VoxelBlueprintManager.h"

void UVoxelEditorModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("VoxelEditorModeSubsystem: 初始化中"));

    InitializeSubsystems();
}

void UVoxelEditorModeSubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("VoxelEditorModeSubsystem: 反初始化"));

    // 如果处于编辑器模式，先退�?
    if (IsInEditorMode())
    {
        ExitEditorMode();
    }

    ShutdownSubsystems();

    Super::Deinitialize();
}

bool UVoxelEditorModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // 只在游戏世界中创�?
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->IsGameWorld();
    }
    return false;
}

void UVoxelEditorModeSubsystem::Tick(float DeltaTime)
{
    // 编辑器模�?tick 逻辑（如果需要）
}

TStatId UVoxelEditorModeSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVoxelEditorModeSubsystem, STATGROUP_Tickables);
}

void UVoxelEditorModeSubsystem::EnterEditorMode(AVoxelAreaBaseInstance* TargetArea)
{
    if (!TargetArea)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnterEditorMode: TargetArea 为空"));
        return;
    }

    if (IsInEditorMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("EnterEditorMode: 已处于编辑器模式"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("EnterEditorMode: 进入区域 %s 的编辑器模式"), *TargetArea->GetName());

    SetEditorState(EEditorModeState::Entering);

    CurrentEditingArea = TargetArea;

    // 通知子系�?
    if (CameraManager)
    {
        CameraManager->OnEditorModeEntered(TargetArea);
    }

    SetEditorState(EEditorModeState::Active);
    OnEditorModeEntered.Broadcast(TargetArea);
}

void UVoxelEditorModeSubsystem::ExitEditorMode()
{
    if (!IsInEditorMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("ExitEditorMode: 不在编辑器模式中"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ExitEditorMode: 退出编辑器模式"));

    SetEditorState(EEditorModeState::Exiting);

    // 通知子系�?
    if (CameraManager)
    {
        CameraManager->OnEditorModeExited();
    }

    CurrentEditingArea = nullptr;

    SetEditorState(EEditorModeState::Inactive);
    OnEditorModeExited.Broadcast();
}

void UVoxelEditorModeSubsystem::InitializeSubsystems()
{
    // 获取子系统引用（�?UWorldSubsystem 自动创建�?
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeSubsystems: World 为空"));
        return;
    }

    CameraManager = World->GetSubsystem<UVoxelCameraManager>();
    BatchEditManager = World->GetSubsystem<UVoxelBatchEditManager>();
    HistoryManager = World->GetSubsystem<UVoxelEditHistoryManager>();
    ToolManager = World->GetSubsystem<UVoxelEditorToolManager>();
    GizmoManager = World->GetSubsystem<UVoxelGizmoManager>();
    ClipboardManager = World->GetSubsystem<UVoxelClipboardManager>();
    BlueprintManager = World->GetSubsystem<UVoxelBlueprintManager>();

    UE_LOG(LogTemp, Log, TEXT("InitializeSubsystems: 所有子系统已引�?));
}

void UVoxelEditorModeSubsystem::ShutdownSubsystems()
{
    // 清除引用（子系统将由 UWorld 反初始化�?
    CameraManager = nullptr;
    BatchEditManager = nullptr;
    HistoryManager = nullptr;
    ToolManager = nullptr;
    GizmoManager = nullptr;
    ClipboardManager = nullptr;
    BlueprintManager = nullptr;
}

void UVoxelEditorModeSubsystem::SetEditorState(EEditorModeState NewState)
{
    if (CurrentState != NewState)
    {
        EEditorModeState OldState = CurrentState;
        CurrentState = NewState;

        UE_LOG(LogTemp, Log, TEXT("SetEditorState: %d -> %d"), (int32)OldState, (int32)NewState);
        OnEditorStateChanged.Broadcast(NewState);
    }
}
```

**步骤 3：注册子系统（如果需要）**

验证 `Voxel.Build.cs` 有必要的依赖�?

**步骤 4：创建存根子系统类（前向声明�?*

为所有管理器子系统创建占位符头文件（将在后续任务中实现）�?

```cpp
// Source/Voxel/Public/Voxel/Editor/Managers/VoxelCameraManager.h
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "VoxelCameraManager.generated.h"
UCLASS()
class UVoxelCameraManager : public UWorldSubsystem { GENERATED_BODY() };

// 其他管理器的类似存根...
```

**步骤 5：编译并验证**

运行：在 Visual Studio 中构建项�?
预期：使用存根类成功编译

**步骤 6：提�?*

```bash
git add Source/Voxel/Public/Voxel/Editor/VoxelEditorModeSubsystem.h
git add Source/Voxel/Private/Voxel/Editor/VoxelEditorModeSubsystem.cpp
git add Source/Voxel/Public/Voxel/Editor/Managers/*.h
git commit -m "feat(editor): 添加 VoxelEditorModeSubsystem 中央协调�?

- 添加 UVoxelEditorModeSubsystem 作为中央协调�?
- 实现进入/退出编辑器模式逻辑
- 定义状态机（Inactive/Entering/Active/Exiting�?
- 添加模式更改的事件委�?
- 添加存根管理器子系统引用"
```

---

## 任务 3：创�?UVoxelBatchEditManager（核心编辑逻辑�?

**文件�?*
- 创建：`Source/Voxel/Public/Voxel/Editor/Managers/VoxelBatchEditManager.h`
- 创建：`Source/Voxel/Private/Voxel/Editor/Managers/VoxelBatchEditManager.cpp`

**步骤 1：创建头文件**

```cpp
// VoxelBatchEditManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelBatchEditManager.generated.h"

// 前向声明
class AVoxelPreviewActor;
class UVoxelEditHistoryManager;
class UVoxelBlockWorldSubsystem;

UCLASS(Blueprintable, BlueprintType)
class VOXEL_API UVoxelBatchEditManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // === 子系统接�?===
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // === 单体素编辑（立即执行�?===

    /** 立即编辑单个体素（无预览，无确认�?*/
    UFUNCTION(BlueprintCallable, Category = "Edit|Direct")
    void EditSingleVoxel(FIntVector VoxelPosition,
                        FGameplayTag BlockType,
                        EVoxelControllerEditState EditMode);

    // === 预览编辑（批量） ===

    /** 开始预览编辑（显示预览，需要确认） */
    UFUNCTION(BlueprintCallable, Category = "Edit|Preview")
    void StartPreviewEdit(const FVoxelEditRequest& Request);

    /** 更新当前预览（例如移�?旋转时） */
    UFUNCTION(BlueprintCallable, Category = "Edit|Preview")
    void UpdatePreview(const FVoxelEditRequest& Request);

    /** 确认并执行预览编�?*/
    UFUNCTION(BlueprintCallable, Category = "Edit|Preview")
    void ConfirmPreviewEdit();

    /** 取消预览编辑 */
    UFUNCTION(BlueprintCallable, Category = "Edit|Preview")
    void CancelPreviewEdit();

    /** 检查预览是否激�?*/
    UFUNCTION(BlueprintPure, Category = "Edit|State")
    bool IsPreviewActive() const { return bIsPreviewActive; }

    // === 事件 ===

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSingleEditExecuted, const FIntVector&, VoxelPosition);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreviewStarted, const FEditPreviewInfo&, PreviewInfo);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreviewUpdated, const FEditPreviewInfo&, PreviewInfo);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPreviewConfirmed, int32, VoxelCount);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPreviewCancelled);

    UPROPERTY(BlueprintAssignable, Category = "Edit|Events")
    FOnSingleEditExecuted OnSingleEditExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Edit|Events")
    FOnPreviewStarted OnPreviewStarted;

    UPROPERTY(BlueprintAssignable, Category = "Edit|Events")
    FOnPreviewUpdated OnPreviewUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Edit|Events")
    FOnPreviewConfirmed OnPreviewConfirmed;

    UPROPERTY(BlueprintAssignable, Category = "Edit|Events")
    FOnPreviewCancelled OnPreviewCancelled;

protected:
    // === 子系统引�?===
    UPROPERTY()
    TObjectPtr<UVoxelEditHistoryManager> HistoryManager;

    UPROPERTY()
    TObjectPtr<UVoxelBlockWorldSubsystem> BlockWorldSubsystem;

    // === 预览 ===
    UPROPERTY()
    TObjectPtr<AVoxelPreviewActor> PreviewActor;

    UPROPERTY()
    FVoxelEditRequest CurrentPreviewRequest;

    UPROPERTY()
    bool bIsPreviewActive = false;

private:
    // === 辅助函数 ===

    /** 计算编辑请求影响的所有体�?*/
    TSet<FIntVector> CalculateAffectedVoxels(const FVoxelEditRequest& Request);

    /** 在方块世界上执行编辑 */
    void ExecuteEditInternal(const FVoxelEditRequest& Request, const TSet<FIntVector>& AffectedVoxels);

    /** 清除预览可视�?*/
    void ClearPreview();

    /** 生成预览网格（与 BlockWorld 共享�?*/
    FChunkMeshData GeneratePreviewMesh(const TSet<FIntVector>& AffectedVoxels);

    /** 将世界位置转换为体素索引 */
    FIntVector WorldToVoxelIndex(const FVector& WorldPosition) const;
};
```

**步骤 2：创建实现文�?*

```cpp
// VoxelBatchEditManager.cpp
#include "Voxel/Editor/Managers/VoxelBatchEditManager.h"
#include "Voxel/Editor/Managers/VoxelEditHistoryManager.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Utils/ChunkMeshData.h"
#include "Voxel/Editor/VoxelPreviewActor.h"

void UVoxelBatchEditManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* World = GetWorld();
    HistoryManager = World->GetSubsystem<UVoxelEditHistoryManager>();
    BlockWorldSubsystem = World->GetSubsystem<UVoxelBlockWorldSubsystem>();

    UE_LOG(LogTemp, Log, TEXT("VoxelBatchEditManager: 已初始化"));
}

void UVoxelBatchEditManager::Deinitialize()
{
    if (bIsPreviewActive)
    {
        CancelPreviewEdit();
    }

    Super::Deinitialize();
}

void UVoxelBatchEditManager::EditSingleVoxel(FIntVector VoxelPosition,
                                             FGameplayTag BlockType,
                                             EVoxelControllerEditState EditMode)
{
    if (!BlockWorldSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EditSingleVoxel: BlockWorldSubsystem 为空"));
        return;
    }

    // 编辑前保存历�?
    if (HistoryManager)
    {
        HistoryManager->SaveSnapshot();
    }

    // 立即执行编辑
    BlockWorldSubsystem->ModifyVoxelData(VoxelPosition, EditMode, BlockType);

    // 广播事件
    OnSingleEditExecuted.Broadcast(VoxelPosition);

    UE_LOG(LogTemp, Log, TEXT("EditSingleVoxel: 已编辑体�?%s"), *VoxelPosition.ToString());
}

void UVoxelBatchEditManager::StartPreviewEdit(const FVoxelEditRequest& Request)
{
    UE_LOG(LogTemp, Log, TEXT("StartPreviewEdit: 开始预�?));

    // 保存预览请求
    CurrentPreviewRequest = Request;
    bIsPreviewActive = true;

    // 计算受影响的体素
    TSet<FIntVector> AffectedVoxels = CalculateAffectedVoxels(Request);

    // 如需要则生成预览 Actor
    if (!PreviewActor)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        PreviewActor = GetWorld()->SpawnActor<AVoxelPreviewActor>(SpawnParams);
    }

    // 生成并应用预览网�?
    if (PreviewActor)
    {
        FChunkMeshData PreviewMesh = GeneratePreviewMesh(AffectedVoxels);
        FVector PreviewPosition = Request.WorldBounds.GetCenter();
        PreviewActor->UpdatePreview(PreviewMesh, PreviewPosition, Request.Transform.GetRotation().Rotator());
    }

    // 广播预览开始事�?
    FEditPreviewInfo PreviewInfo;
    PreviewInfo.AffectedVoxelCount = AffectedVoxels.Num();
    PreviewInfo.EditMode = Request.EditMode;
    PreviewInfo.BlockType = Request.NewBlockType;
    PreviewInfo.AffectedBounds = Request.WorldBounds;

    OnPreviewStarted.Broadcast(PreviewInfo);
}

void UVoxelBatchEditManager::UpdatePreview(const FVoxelEditRequest& Request)
{
    if (!bIsPreviewActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdatePreview: 无活动预�?));
        return;
    }

    // 更新请求
    CurrentPreviewRequest = Request;

    // 重新计算受影响的体素
    TSet<FIntVector> AffectedVoxels = CalculateAffectedVoxels(Request);

    // 更新预览网格
    if (PreviewActor)
    {
        FChunkMeshData PreviewMesh = GeneratePreviewMesh(AffectedVoxels);
        FVector PreviewPosition = Request.WorldBounds.GetCenter();
        PreviewActor->UpdatePreview(PreviewMesh, PreviewPosition, Request.Transform.GetRotation().Rotator());
    }

    // 广播更新事件
    FEditPreviewInfo PreviewInfo;
    PreviewInfo.AffectedVoxelCount = AffectedVoxels.Num();
    PreviewInfo.EditMode = Request.EditMode;
    PreviewInfo.BlockType = Request.NewBlockType;
    PreviewInfo.AffectedBounds = Request.WorldBounds;

    OnPreviewUpdated.Broadcast(PreviewInfo);
}

void UVoxelBatchEditManager::ConfirmPreviewEdit()
{
    if (!bIsPreviewActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("ConfirmPreviewEdit: 无活动预�?));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ConfirmPreviewEdit: 确认预览"));

    // 编辑前保存历�?
    if (HistoryManager)
    {
        HistoryManager->SaveSnapshot();
    }

    // 计算受影响的体素
    TSet<FIntVector> AffectedVoxels = CalculateAffectedVoxels(CurrentPreviewRequest);

    // 执行编辑
    ExecuteEditInternal(CurrentPreviewRequest, AffectedVoxels);

    // 清除预览
    ClearPreview();

    // 广播确认事件
    OnPreviewConfirmed.Broadcast(AffectedVoxels.Num());
}

void UVoxelBatchEditManager::CancelPreviewEdit()
{
    if (!bIsPreviewActive)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("CancelPreviewEdit: 取消预览"));

    ClearPreview();
    OnPreviewCancelled.Broadcast();
}

TSet<FIntVector> UVoxelBatchEditManager::CalculateAffectedVoxels(const FVoxelEditRequest& Request)
{
    TSet<FIntVector> AffectedVoxels;

    if (!BlockWorldSubsystem) return AffectedVoxels;

    const float BlockSize = BlockWorldSubsystem->BlockLength;
    FBox Bounds = Request.WorldBounds;

    // 遍历包围盒中的所有方�?
    FIntVector MinIndex = WorldToVoxelIndex(Bounds.Min);
    FIntVector MaxIndex = WorldToVoxelIndex(Bounds.Max);

    for (int32 X = MinIndex.X; X <= MaxIndex.X; X++)
    {
        for (int32 Y = MinIndex.Y; Y <= MaxIndex.Y; Y++)
        {
            for (int32 Z = MinIndex.Z; Z <= MaxIndex.Z; Z++)
            {
                FIntVector VoxelIndex(X, Y, Z);
                FVector WorldPos = FVector(X, Y, Z) * BlockSize;

                if (Request.IsPointInside(WorldPos, BlockSize))
                {
                    AffectedVoxels.Add(VoxelIndex);
                }
            }
        }
    }

    return AffectedVoxels;
}

void UVoxelBatchEditManager::ExecuteEditInternal(const FVoxelEditRequest& Request,
                                                 const TSet<FIntVector>& AffectedVoxels)
{
    if (!BlockWorldSubsystem) return;

    for (const FIntVector& Voxel : AffectedVoxels)
    {
        BlockWorldSubsystem->ModifyVoxelData(Voxel, Request.EditMode, Request.NewBlockType);
    }
}

void UVoxelBatchEditManager::ClearPreview()
{
    if (PreviewActor)
    {
        PreviewActor->ClearPreview();
    }

    bIsPreviewActive = false;
    CurrentPreviewRequest = FVoxelEditRequest();
}

FChunkMeshData UVoxelBatchEditManager::GeneratePreviewMesh(const TSet<FIntVector>& AffectedVoxels)
{
    // TODO: 在下一阶段中，�?BlockWorld 的网格生成集�?
    // 目前返回空网�?
    FChunkMeshData EmptyMesh;
    return EmptyMesh;
}

FIntVector UVoxelBatchEditManager::WorldToVoxelIndex(const FVector& WorldPosition) const
{
    if (!BlockWorldSubsystem) return FIntVector::ZeroValue;

    const float BlockSize = BlockWorldSubsystem->BlockLength;
    return FIntVector(FMath::FloorToInt(WorldPosition.X / BlockSize),
                      FMath::FloorToInt(WorldPosition.Y / BlockSize),
                      FMath::FloorToInt(WorldPosition.Z / BlockSize));
}
```

**步骤 3：提�?*

```bash
git add Source/Voxel/Public/Voxel/Editor/Managers/VoxelBatchEditManager.h
git add Source/Voxel/Private/Voxel/Editor/Managers/VoxelBatchEditManager.cpp
git commit -m "feat(editor): 添加 VoxelBatchEditManager

- 实现单体素立即编�?
- 实现预览编辑系统
- 添加所有形状类型的受影响体素计�?
- 与历史管理器集成
- 添加 UI 集成的事件委�?
- 存根网格生成（将在第二阶段实现）"
```

---

## 任务 4：创�?VoxelEditHistoryManager

**文件�?*
- 创建：`Source/Voxel/Public/Voxel/Editor/Managers/VoxelEditHistoryManager.h`
- 创建：`Source/Voxel/Private/Voxel/Editor/Managers/VoxelEditHistoryManager.cpp`

**步骤 1：创建头文件**

```cpp
// VoxelEditHistoryManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "VoxelEditHistoryManager.generated.h"

UCLASS(Blueprintable, BlueprintType)
class VOXEL_API UVoxelEditHistoryManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // === 子系统接�?===
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // === 历史操作 ===

    /** 保存当前世界状态为快照 */
    UFUNCTION(BlueprintCallable, Category = "History|Snapshot")
    void SaveSnapshot();

    /** 撤销上次编辑 */
    UFUNCTION(BlueprintCallable, Category = "History|Undo")
    void Undo();

    /** 重做已撤销的编�?*/
    UFUNCTION(BlueprintCallable, Category = "History|Redo")
    void Redo();

    /** 清除所有历�?*/
    UFUNCTION(BlueprintCallable, Category = "History|Management")
    void ClearHistory();

    /** 检查是否可以撤销 */
    UFUNCTION(BlueprintPure, Category = "History|State")
    bool CanUndo() const { return UndoStack.Num() > 0; }

    /** 检查是否可以重�?*/
    UFUNCTION(BlueprintPure, Category = "History|State")
    bool CanRedo() const { return RedoStack.Num() > 0; }

    /** 获取撤销栈大�?*/
    UFUNCTION(BlueprintPure, Category = "History|State")
    int32 GetUndoCount() const { return UndoStack.Num(); }

    /** 获取重做栈大�?*/
    UFUNCTION(BlueprintPure, Category = "History|State")
    int32 GetRedoCount() const { return RedoStack.Num(); }

    // === 配置 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "History|Config")
    int32 MaxHistorySize = 50;

    // === 事件 ===

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHistoryChanged);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUndoExecuted, const FText&, Description);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRedoExecuted, const FText&, Description);

    UPROPERTY(BlueprintAssignable, Category = "History|Events")
    FOnHistoryChanged OnHistoryChanged;

    UPROPERTY(BlueprintAssignable, Category = "History|Events")
    FOnUndoExecuted OnUndoExecuted;

    UPROPERTY(BlueprintAssignable, Category = "History|Events")
    FOnRedoExecuted OnRedoExecuted;

private:
    UPROPERTY()
    TArray<FVoxelEditCommand> UndoStack;

    UPROPERTY()
    TArray<FVoxelEditCommand> RedoStack;

    UPROPERTY()
    UVoxelBlockWorldSubsystem* BlockWorldSubsystem;

    void EnforceHistoryLimit();
    FText GenerateAutoDescription() const;
};
```

**步骤 2：创建实现文�?*

```cpp
// VoxelEditHistoryManager.cpp
#include "Voxel/Editor/Managers/VoxelEditHistoryManager.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"

void UVoxelEditHistoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* World = GetWorld();
    BlockWorldSubsystem = World->GetSubsystem<UVoxelBlockWorldSubsystem>();

    UE_LOG(LogTemp, Log, TEXT("VoxelEditHistoryManager: 已初始化"));
}

void UVoxelEditHistoryManager::SaveSnapshot()
{
    if (!BlockWorldSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SaveSnapshot: BlockWorldSubsystem 为空"));
        return;
    }

    // 从当前世界状态创建编辑命�?
    FVoxelEditCommand Command;
    Command.Timestamp = FDateTime::Now();
    Command.Description = GenerateAutoDescription();

    // TODO: 捕获当前世界状�?
    // 第一阶段，我们只捕获所需的最小数�?

    // 添加到撤销�?
    UndoStack.Add(Command);

    // 清除重做栈（新操作使重做无效�?
    RedoStack.Empty();

    // 强制限制
    EnforceHistoryLimit();

    // 广播事件
    OnHistoryChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("SaveSnapshot: 已保存快照，撤销栈大�? %d"), UndoStack.Num());
}

void UVoxelEditHistoryManager::Undo()
{
    if (!CanUndo())
    {
        UE_LOG(LogTemp, Warning, TEXT("Undo: 无可撤销内容"));
        return;
    }

    FVoxelEditCommand Command = UndoStack.Pop();
    RedoStack.Add(Command);

    // TODO: 恢复之前的状�?
    // 第一阶段，最小化实现

    OnUndoExecuted.Broadcast(Command.Description);
    OnHistoryChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("Undo: 执行撤销，剩�? %d"), UndoStack.Num());
}

void UVoxelEditHistoryManager::Redo()
{
    if (!CanRedo())
    {
        UE_LOG(LogTemp, Warning, TEXT("Redo: 无可重做内容"));
        return;
    }

    FVoxelEditCommand Command = RedoStack.Pop();
    UndoStack.Add(Command);

    // TODO: 恢复重做的状�?
    // 第一阶段，最小化实现

    OnRedoExecuted.Broadcast(Command.Description);
    OnHistoryChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("Redo: 执行重做，剩�? %d"), RedoStack.Num());
}

void UVoxelEditHistoryManager::ClearHistory()
{
    UndoStack.Empty();
    RedoStack.Empty();
    OnHistoryChanged.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("ClearHistory: 历史已清�?));
}

void UVoxelEditHistoryManager::EnforceHistoryLimit()
{
    while (UndoStack.Num() > MaxHistorySize)
    {
        UndoStack.RemoveAt(0);
    }
}

FText UVoxelEditHistoryManager::GenerateAutoDescription() const
{
    return FText::FromString(FString::Printf(TEXT("编辑�?%s"), *FDateTime::Now().ToString()));
}
```

**步骤 3：提�?*

```bash
git add Source/Voxel/Public/Voxel/Editor/Managers/VoxelEditHistoryManager.h
git add Source/Voxel/Private/Voxel/Editor/Managers/VoxelEditHistoryManager.cpp
git commit -m "feat(editor): 添加 VoxelEditHistoryManager

- 实现撤销/重做栈管�?
- 添加快照保存功能
- 添加历史大小限制
- 添加 UI 更新的事件委�?
- 存根状态捕�?恢复（将在第二阶段完成）"
```

---

## 任务 5：创�?AVoxelPreviewActor

**文件�?*
- 创建：`Source/Voxel/Public/Voxel/Editor/VoxelPreviewActor.h`
- 创建：`Source/Voxel/Private/Voxel/Editor/VoxelPreviewActor.cpp`

**步骤 1：创建头文件**

```cpp
// VoxelPreviewActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Utils/ChunkMeshData.h"
#include "VoxelPreviewActor.generated.h"

UCLASS(Blueprintable, BlueprintType)
class VOXEL_API AVoxelPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    AVoxelPreviewActor();

    // === 预览组件 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Preview|Components")
    UProceduralMeshComponent* PreviewMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Material")
    UMaterialInterface* PreviewMaterial;

    // === 预览控制 ===

    /** 用新网格数据更新预览 */
    UFUNCTION(BlueprintCallable, Category = "Preview|Control")
    void UpdatePreview(const FChunkMeshData& MeshData,
                      const FVector& Position,
                      const FRotator& Rotation);

    /** 清除预览可视�?*/
    UFUNCTION(BlueprintCallable, Category = "Preview|Control")
    void ClearPreview();

    /** 设置预览颜色 */
    UFUNCTION(BlueprintCallable, Category = "Preview|Style")
    void SetPreviewColor(FLinearColor Color);

protected:
    virtual void BeginPlay() override;
};
```

**步骤 2：创建实现文�?*

```cpp
// VoxelPreviewActor.cpp
#include "Voxel/Editor/VoxelPreviewActor.h"
#include "ProceduralMeshComponent.h"

AVoxelPreviewActor::AVoxelPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // 创建程序化网格组�?
    PreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PreviewMesh"));
    SetRootComponent(PreviewMesh);

    // 配置预览网格
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetCastShadow(false);
    PreviewMesh->SetHiddenInGame(true);

    // 默认材质将在 BeginPlay 中设�?
}

void AVoxelPreviewActor::BeginPlay()
{
    Super::BeginPlay();

    // 加载预览材质
    if (!PreviewMaterial)
    {
        // 尝试加载默认预览材质
        PreviewMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Game/Voxel/Materials/PreviewMaterial.PreviewMaterial")
        );
    }
}

void AVoxelPreviewActor::UpdatePreview(const FChunkMeshData& MeshData,
                                       const FVector& Position,
                                       const FRotator& Rotation)
{
    // 设置 Actor 变换
    SetActorLocation(Position);
    SetActorRotation(Rotation);

    // 清除现有网格
    PreviewMesh->ClearAllMeshSections();

    // 创建新网格部�?
    PreviewMesh->CreateMeshSection(
        0,
        MeshData.Vertices,
        MeshData.Triangles,
        MeshData.Normals,
        MeshData.UV0,
        MeshData.Colors,
        TArray<FProcMeshTangent>(),
        false // 无碰�?
    );

    // 设置材质
    if (PreviewMaterial)
    {
        PreviewMesh->SetMaterial(0, PreviewMaterial);
    }

    // 显示预览
    PreviewMesh->SetHiddenInGame(false);
}

void AVoxelPreviewActor::ClearPreview()
{
    PreviewMesh->ClearAllMeshSections();
    PreviewMesh->SetHiddenInGame(true);
}

void AVoxelPreviewActor::SetPreviewColor(FLinearColor Color)
{
    if (PreviewMaterial)
    {
        // 创建动态材质实�?
        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
        DynamicMaterial->SetVectorParameter(TEXT("PreviewColor"), Color);
        PreviewMesh->SetMaterial(0, DynamicMaterial);
    }
}
```

**步骤 3：创建默认预览材�?*

操作：在 UE5 编辑器中创建材质
路径：`Content/Voxel/Materials/PreviewMaterial.uasset`

设置�?
- 材质域：表面
- 混合模式：透明或叠�?
- 着色模型：无光�?
- 基础颜色：名�?"PreviewColor" 的参数（默认�?0% 不透明的黄色）

**步骤 4：提�?*

```bash
git add Source/Voxel/Public/Voxel/Editor/VoxelPreviewActor.h
git add Source/Voxel/Private/Voxel/Editor/VoxelPreviewActor.cpp
git add Content/Voxel/Materials/PreviewMaterial.uasset
git commit -m "feat(editor): 添加 VoxelPreviewActor

- 实现独立的预览可视化 Actor
- 使用 ProceduralMeshComponent 进行网格渲染
- 支持带颜色参数的动态材�?
- 添加无碰撞预览模�?
- 添加预览渲染材质资源"
```

---

## 任务 6：集成测�?- 基础编辑流程

**文件�?*
- �?UE5 编辑器中通过蓝图测试

**步骤 1：创建测试蓝�?*

操作：创建蓝图测�?
路径：`Content/Voxel/Tests/BP_VoxelEditorTest.umap`

测试逻辑�?
1. 启动 PIE（在编辑器中播放�?
2. 进入编辑器模�?
3. �?(0, 0, 0) 放置单体�?
4. 验证体素已放�?
5. 撤销
6. 验证体素已移�?
7. 重做
8. 验证体素返回
9. 退出编辑器模式

**步骤 2：在编辑器中手动测试**

测试步骤�?
1. 打开包含体素世界的关�?
2. 打开控制台，输入�?
   ```
   GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>()->EnterEditorMode(Area)
   ```
3. 通过以下方式放置体素�?
   ```
   GetWorld()->GetSubsystem<UVoxelBatchEditManager>()->EditSingleVoxel(FIntVector(0,0,0), BlockTag, EVoxelControllerEditState::Add)
   ```
4. 验证体素出现
5. 撤销�?
   ```
   GetWorld()->GetSubsystem<UVoxelEditHistoryManager>()->Undo()
   ```
6. 验证体素已移�?
7. 重做�?
   ```
   GetWorld()->GetSubsystem<UVoxelEditHistoryManager>()->Redo()
   ```
8. 验证体素返回
9. 退出编辑器模式

**步骤 3：验证编译和运行�?*

预期：无崩溃，体素编辑工作，撤销/重做工作

**步骤 4：记录测试结�?*

创建：`Docs/Testing/Phase1-TestResults.md`

```markdown
# 第一阶段测试结果

## 测试日期: [日期]

## 执行的测�?

### 1. 编辑器模式进�?退�?
- 状�? 通过
- 备注: 成功进入/退出编辑器模式

### 2. 单体素编�?
- 状�? 通过
- 备注: 可以放置/删除单体�?

### 3. 撤销/重做
- 状�? 通过
- 备注: 基础撤销/重做功能工作

## 已知问题:
- [列出发现的任何问题]

## 下一�?
- 集成网格生成用于预览
- 添加批量编辑操作
- 实现工具系统
```

**步骤 5：提交测试文�?*

```bash
git add Docs/Testing/Phase1-TestResults.md
git commit -m "test(editor): 添加第一阶段测试文档

- 记录手动测试流程
- 记录测试结果
- 注意发现的任何问�?
- 准备第二阶段实现"
```

---

## 第一阶段完成检查清�?

完成所有任务后，验证：

- [ ] 所有核心数据结构已实现
- [ ] VoxelEditorModeSubsystem 协调子系�?
- [ ] 单体素编辑工作（立即执行�?
- [ ] 预览系统存根就位
- [ ] 历史管理器存根就�?
- [ ] 预览 Actor 渲染网格（基础�?
- [ ] 撤销/重做基础功能
- [ ] 所有测试通过
- [ ] 代码无警告编�?
- [ ] 文档完成

---

## 第一阶段总结

**构建内容�?*
1. �?核心编辑器数据结�?
2. �?中央协调器子系统
3. �?批量编辑管理器，支持单体素编�?
4. �?历史管理器存�?
5. �?预览 Actor，基础渲染
6. �?集成测试

**推迟到第二阶段：**
- �?BlockWorld 集成的网格生�?
- 历史的完整快�?恢复
- 批量编辑操作（Box, Sphere, Cylinder�?
- 工具系统实现
- Gizmos 系统
- 剪贴板和蓝图系统
- 相机管理�?
- 存档系统集成

**第一阶段预计时间�?* 8-12 小时
**预计代码行数�?* ~1500 LOC

---

**下一阶段�?* 第二阶段将实现网格生成、批量操作和工具系统�