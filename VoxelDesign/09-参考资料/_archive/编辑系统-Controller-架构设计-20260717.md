# 编辑系统 Controller 架构设计

> **文档目的**：定义VoxelPlayerController的整体架构、组件关系和接口设计
> **创建日期**：2026-01-16
> **版本**：v1.0
> **状态**：已确定

---

## 一、概述

### 1.1 Controller定位

**VoxelPlayerController**是玩家控制器，负责协调输入检测、业务处理和渲染更新。

**核心定位**：
- 是玩家控制的入口点
- 持有和管理多个Component
- 协调各Component之间的交互
- 不直接处理业务逻辑和渲染

### 1.2 设计原则

1. **职责分离**：每个Component有明确的职责
2. **单向数据流**：业务数据 → 渲染指令 → 渲染执行
3. **输入统一**：蓝图检测输入并提供体素索引，InteractComponent管理输入状态
4. **弱依赖**：组件间通过接口通信，减少直接依赖

---

## 二、整体架构

### 2.1 三层架构

```
VoxelPlayerController（协调层）
    ↓
蓝图（输入检测层，计算体素索引）
    ↓
InteractComponent（业务数据处理层 + 输入状态存储）
    ├─→ StateComponent（读写业务数据）
    ├─→ WorldSubsystem（执行编辑）
    ├─→ HistoryComponent（记录历史）
    └─→ VisualUIComponent（渲染处理层）
           ↓
       PreviewActor（渲染执行层）
```

### 2.2 组件关系图

```
AVoxelPlayerController
├─ UVoxelControllerStateComponent（数据存储）
├─ UVoxelControllerInteractComponent（业务逻辑 + 输入状态）
├─ UVoxelControllerVisualUIComponent（渲染协调）
├─ UVoxelControllerHistoryComponent（历史记录）
└─ UVoxelControllerXXXComponent（其他功能组件）
```

### 2.3 数据流向

```
用户输入
    ↓
蓝图（射线检测，计算体素索引）
    ↓
InteractComponent（处理业务数据 + 管理输入状态）
    ├─→ StateComponent（读写业务数据）
    ├─→ WorldSubsystem（执行编辑）
    ├─→ HistoryComponent（记录历史）
    └─→ VisualUIComponent（处理渲染）
           ├─→ StateComponent（读取业务数据）
           └─→ PreviewActor（执行渲染）
```

---

## 三、核心组件设计

### 3.1 UVoxelControllerStateComponent

#### 职责
- **存储所有玩家状态数据**
- **提供状态访问器和修改器**（包含状态验证和业务逻辑）
- **广播状态变更事件**（细粒度事件系统）
- 管理临时区域信息和临时几何信息（分离存储）
- 管理编辑请求的动态组装

#### 实现说明（与理想设计的差异）

**⚠️ 实际实现偏离了纯数据容器的设计**：

在实际代码实现中（`VoxelControllerStateComponent.h` 第 213-325 行），StateComponent 包含了业务逻辑方法：

```cpp
// 这些方法在实际实现中存在，但违反了纯数据管理原则
EVoxelControllerMode GetPlayerMode() const;
void SetPlayerMode(EVoxelControllerMode NewMode);
EVoxelControllerBuildState GetBuildState() const;
void SetBuildState(EVoxelControllerBuildState NewState);
FVoxelGeometryDesc GetTmpGeometry() const;
void SetTmpGeometry(const FVoxelGeometryDesc& Geometry);
FVoxelEditRequest GetTmpEditRequest() const;
void ClearTmpGeometry();
```

**实际实现特点**：
- ✅ 包含状态存储和访问
- ✅ 包含状态验证和业务逻辑
- ⚠️ 违反了单一职责原则（SRP）
- ⚠️ 业务逻辑散落在 StateComponent 和 InteractComponent 之间

**为什么不修复**：
- 代码已经稳定运行
- 修改风险高、收益低
- 蓝图已依赖这些接口

#### 不做
- 不检测输入（蓝图处理）
- 不渲染（由 VisualUIComponent 处理）
- 不执行编辑操作（由 InteractComponent 处理）

#### 性能优化设计
- **分离存储**: 将区域信息和几何信息分开存储，避免数据冗余
- **细粒度事件**: 不同类型的数据变更使用不同的事件，避免不必要的广播
- **动态组装**: `FVoxelEditRequest` 在访问时动态组装，而非冗余存储

#### 存储的数据

```cpp
/**
 * 状态存储组件
 *
 * 职责：
 * - 存储所有玩家状态数据
 * - 提供状态访问器和修改器
 * - 广播状态变更事件
 *
 * 不做：
 * - 不处理业务逻辑
 * - 不检测输入
 * - 不渲染
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UVoxelControllerStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // === 控制器模式 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Mode")
    EVoxelControllerMode PlayerMode;

    // === 建造状态 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Build")
    EVoxelControllerBuildState BuildState;

    // === 编辑模式 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Edit")
    EVoxelControllerEditState EditState;

    // === 批量编辑 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Batch")
    bool bBatchMode;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Batch")
    EBatchShapeType CurrentBatchShape;

    // === 临时区域信息（分离存储） ===

    UPROPERTY(VisibleInstanceOnly, Category = "State|Area")
    FTemporaryAreaInfo TmpAreaInfo;

    // === 临时几何信息（包含 Coverage） ===

    UPROPERTY(VisibleInstanceOnly, Category = "State|Geometry")
    FVoxelGeometryDesc TmpGeometry;

    // === 手持物品 ===

    UPROPERTY(VisibleInstanceOnly, Category = "State|Item")
    FInteractableData InteractableDataInHand;

    // === 当前区域（可选） ===

    UPROPERTY(VisibleInstanceOnly, Category = "State|Area")
    AVoxelAreaBaseInstance* CurArea;

    // === 当前父区域 ===

    UPROPERTY(VisibleInstanceOnly, Category = "State|Area")
    AVoxelAreaBaseInstance* CurParentArea;

public:
    // === 访问器 ===

    UFUNCTION(BlueprintPure, Category = "Voxel|State")
    EVoxelControllerMode GetPlayerMode() const { return PlayerMode; }

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    void SetPlayerMode(EVoxelControllerMode NewMode);

    UFUNCTION(BlueprintPure, Category = "Voxel|State")
    EVoxelControllerBuildState GetBuildState() const { return BuildState; }

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    void SetBuildState(EVoxelControllerBuildState NewState);

    UFUNCTION(BlueprintPure, Category = "Voxel|State")
    EVoxelControllerEditState GetEditState() const { return EditState; }

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    void SetEditState(EVoxelControllerEditState NewState);

    // === 临时几何信息 ===

    UFUNCTION(BlueprintPure, Category = "Voxel|State")
    FVoxelGeometryDesc GetTmpGeometry() const { return TmpGeometry; }

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    void SetTmpGeometry(const FVoxelGeometryDesc& Geometry);

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    void ClearTmpGeometry();

    // === 临时编辑请求 ===

    UFUNCTION(BlueprintCallable, Category = "Voxel|State")
    FVoxelEditRequest GetTmpEditRequest() const;

    // === 状态变更事件（细粒度事件系统） ===

    // 玩家模式事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControllerModeChanged, UVoxelControllerStateComponent*, SC);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Player")
    FOnControllerModeChanged OnControllerModeChanged;

    // 构建状态事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildStateChanged, UVoxelControllerStateComponent*, SC);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Build")
    FOnBuildStateChanged OnBuildStateChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditStateChanged, EVoxelControllerEditState, NewState);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Build")
    FOnEditStateChanged OnEditStateChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBatchModeChanged, bool, NewState);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Build")
    FOnBatchModeChanged OnBatchModeChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBatchShapeChanged, EBatchShapeType, NewShape);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Build")
    FOnBatchShapeChanged OnBatchShapeChanged;

    // 物品事件
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemInHandChanged, FInteractableData&, Data);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Item")
    FOnItemInHandChanged OnItemInHandChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemInSightChanged, FInteractableData&, Data);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Item")
    FOnItemInSightChanged OnItemInSightChanged;

    // 区域事件（细粒度）
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrentAreaChanged, AVoxelAreaBaseInstance*, OldArea, AVoxelAreaBaseInstance*, NewArea);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Area")
    FOnCurrentAreaChanged OnCurrentAreaChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTmpEditRequestChanged, const FVoxelEditRequest&, NewRequest);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Area|Geometry")
    FOnTmpEditRequestChanged OnTmpEditRequestChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAreaFieldChanged, const FTemporaryAreaInfo&, NewAreaInfo);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Area|Field")
    FOnAreaFieldChanged OnAreaFieldChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAreaModificationChanged, const FAreaModificationData&, NewModificationData);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events|Area|Modification")
    FOnAreaModificationChanged OnAreaModificationChanged;
};
```

---

### 3.2 UVoxelControllerInteractComponent

#### 职责
- **编辑操作**: 处理体素编辑、批量编辑、包围盒计算
- **输入状态管理**: 存储 FirstPoint、SecondPoint（世界坐标）
- **道具操作**: 设置手持道具、家具放置、旋转家具
- **区域操作**: 选择区域、生成区域、进入子区域、返回父区域
- **历史记录操作**: 撤销、重做、清空历史
- **权限检查**: 检查修改成本和权限
- **Gizmo 交互**: 处理平移、旋转、面推拉操作
- 管理业务数据（读写StateComponent）
- 协调 StateComponent、VisualUIComponent、HistoryComponent

#### 不做
- 不进行射线检测（由蓝图处理）
- 不直接渲染（由VisualUIComponent处理）
- 不管理UI元素（由VisualUIComponent管理）

#### 组件引用缓存
```cpp
// 在 BeginPlay 中缓存组件引用，避免频繁查询
UPROPERTY()
TObjectPtr<UVoxelControllerStateComponent> CachedStateComponent;

UPROPERTY()
TObjectPtr<UVoxelControllerVisualUIComponent> CachedVisualUIComponent;

UPROPERTY()
TObjectPtr<UVoxelControllerHistoryComponent> CachedHistoryComponent;
```

#### 接口设计

```cpp
/**
 * 交互处理组件
 *
 * 职责：
 * - 接收蓝图传入的体素索引
 * - 存储输入状态（FirstPoint、SecondPoint）
 * - 处理所有业务逻辑（编辑、道具、区域、历史）
 * - 管理业务数据
 * - 调用WorldSubsystem执行编辑
 *
 * 不做：
 * - 不进行射线检测（蓝图处理）
 * - 不直接渲染
 * - 不管理UI元素
 */
UCLASS(ClassGroup=(Custom), BlueprintType, meta=(BlueprintSpawnableComponent))
class UVoxelControllerInteractComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // ==================== 状态切换 ====================

    /** 切换工具类型 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
    void SwitchTool(EVoxelControllerBuildState NewTool);

    /** 切换编辑模式 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
    void SwitchEditMode(EVoxelControllerEditState NewEditMode);

    /** 设置批量编辑模式 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
    void SetBatchEditMode(bool bEnable);

    /** 设置批量形状 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
    void SetBatchShape(EBatchShapeType NewShape);

    /** 设置玩家模式 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|State")
    void SetPlayerMode(EVoxelControllerMode NewMode);

    // ==================== 编辑操作 ====================

    /** 释放批量编辑预览 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Edit")
    void ReleaseBatchEditPreview();

    /** 取消当前编辑 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Edit")
    void CancelCurrentEdit();

    // ==================== 道具操作 ====================

    /** 设置手持道具 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
    void SetItemInHand(EInteractableType Type, FGameplayTag Id, int Quantity);

    /** 设置视线中的道具 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
    void SetItemInSight(EInteractableType Type, FGameplayTag Id, int Quantity);

    /** 更新家具放置位置 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
    void UpdateFurniturePosition(FIntVector Position);

    /** 旋转家具 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
    void RotateFurniture(EActorDirection Direction);

    /** 获取家具边界 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Item")
    FBox GetFurnitureBounds(FIntVector Position, EActorDirection Direction);

    // ==================== 区域操作 ====================

    /** 选择区域（nullptr 表示取消选择） */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
    void SelectArea(AVoxelAreaBaseInstance* Area);

    /** 生成或更新当前区域 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
    bool CommitArea();

    /** 设置区域类型 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
    void SetAreaType(FGameplayTag AreaTypeTag);

    /** 进入子区域 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
    void EnterSubArea();

    /** 返回父区域 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Area")
    void ReturnToParentArea();

    // ==================== Gizmo 操作 ====================

    /**
     * 处理 Gizmo 交互（统一的 Gizmo 操作入口）
     * @param Input 3D UI Actor 指针（用于识别是哪个 Gizmo，可为 nullptr）
     * @param WorldPosition 世界坐标位置
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Gizmo")
    void HandleGizmoInteraction(AVoxel3DUIActor* Input, FVector WorldPosition);

    /** 释放 Gizmo 交互（清空 FirstPoint/SecondPoint） */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Gizmo")
    void ReleaseGizmoInteraction();

    // ==================== 几何操作 ====================

    /** 设置几何旋转 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
    void SetGeometryRotation(FRotator NewRotation);

    /** 设置几何厚度 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
    void SetGeometryThickness(int32 NewThickness);

    /** 设置规则几何模式 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|Geometry")
    void SetRegularGeometry(bool bEnable);

    // ==================== 历史记录操作 ====================

    /** 撤销上一次编辑 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
    bool Undo();

    /** 重做上次撤销的操作 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
    bool Redo();

    /** 清空编辑历史 */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Interact|History")
    void ClearHistory();

private:
    /** 输入状态存储（世界坐标） */
    UPROPERTY()
    FVector FirstPoint;

    UPROPERTY()
    FVector SecondPoint;

    /** 是否正在修改 */
    bool bIsModyfing = false;

    /** 缓存的状态组件引用 */
    UPROPERTY()
    TObjectPtr<UVoxelControllerStateComponent> CachedStateComponent;

    /** 缓存的可视化 UI 组件引用 */
    UPROPERTY()
    TObjectPtr<UVoxelControllerVisualUIComponent> CachedVisualUIComponent;

    /** 缓存的历史记录组件引用 */
    UPROPERTY()
    TObjectPtr<UVoxelControllerHistoryComponent> CachedHistoryComponent;

    // ==================== 内部辅助方法 ====================

    /** 计算包围盒并更新预览 */
    void CalculateBoundsAndUpdatePreview();

    /** Gizmo 操作：平移 */
    void HandleGizmo_Translate(AVoxel3DUIActor* Input, int AxisIndex);

    /** Gizmo 操作：旋转 */
    void HandleGizmo_Rotate(AVoxel3DUIActor* Input, int32 AxisIndex);

    /** Gizmo 操作：面推拉 */
    void HandleGizmo_Face(AVoxel3DUIActor* Input, int FaceIndex);
};
```

---

### 3.4 UVoxelControllerVisualUIComponent

#### 职责
- **持有和管理所有UI元素**（3D UI Actor + 2D UI Widget）
- **控制所有Actor本身的渲染变动**
- **读取业务数据，转换为渲染指令**
- **Gizmo 渲染管理**: 创建、更新、显示/隐藏 Gizmo
- **Gizmo 位置更新**: 根据 StateComponent 的数据更新 Gizmo 的位置和旋转
- 调用各UI元素更新渲染
- 监听 StateComponent 事件并响应

#### 不做
- 不处理业务逻辑（由InteractComponent处理）
- 不进行射线检测（由蓝图处理）
- 不管理业务数据（由InteractComponent管理）
- **不计算 Gizmo 交互逻辑**（由InteractComponent处理）

#### Gizmo 管理
```cpp
// ==================== Gizmo 实例 ==================

/** 平移 Gizmo 数组（索引对应：0:X, 1:Y, 2:Z） */
UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
TArray<TObjectPtr<AVoxel3DUIActor>> TranslateGizmos;

/** 旋转 Gizmo 数组（索引对应：0:X, 1:Y, 2:Z） */
UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
TArray<TObjectPtr<AVoxel3DUIActor>> RotateGizmos;

/** 面 Gizmo 数组（索引对应：0:-X, 1:+X, 2:-Y, 3:+Y, 4:-Z, 5:+Z） */
UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;

/** 当前激活的 Gizmo 类型 */
UPROPERTY(BlueprintReadOnly, Category = "Voxel|Visual|Gizmo")
EInteractiveGizmoType ActiveGizmoType = EInteractiveGizmoType::None;
```

#### 事件绑定
```cpp
/** 绑定 StateComponent 事件 */
void BindDelegates(UVoxelControllerStateComponent* SC)
{
    SC->OnTmpEditRequestChanged.AddDynamic(this, &ThisClass::HandleTmpEditRequestChanged);
    SC->OnAreaFieldChanged.AddDynamic(this, &ThisClass::HandleAreaFieldChanged);
    SC->OnCurrentAreaChanged.AddDynamic(this, &ThisClass::HandleCurrentAreaChanged);
    SC->OnBuildStateChanged.AddDynamic(this, &ThisClass::HandleBuildStateChanged);
    SC->OnBatchModeChanged.AddDynamic(this, &ThisClass::HandleBatchModeChanged);
}
```

#### 接口设计

```cpp
/**
 * 视觉UI组件
 *
 * 职责：
 * - 持有和管理所有UI元素
 * - 控制所有Actor的渲染变动
 * - 读取业务数据，转换为渲染指令
 * - 调用各UI元素更新渲染
 *
 * 不做：
 * - 不处理业务逻辑
 * - 不检测输入
 * - 不管理业务数据
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UVoxelControllerVisualUIComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // === 3D UI管理 ===

    /**
     * 注册3D UI
     * @param UIActor UI Actor
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
    void Register3DUI(AActor* UIActor);

    /**
     * 注销3D UI
     * @param UIActor UI Actor
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
    void Unregister3DUI(AActor* UIActor);

    /**
     * 显示UI
     * @param UIType UI类型
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
    void ShowUI(EUIType UIType);

    /**
     * 隐藏UI
     * @param UIType UI类型
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual")
    void HideUI(EUIType UIType);

    // === PreviewActor控制 ===

    /**
     * 设置预览状态
     * @param NewState 新的流程状态
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
    void SetPreviewState(EPreviewInteractionState NewState);

    /**
     * 设置预览模式
     * @param NewMode 预览模式
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
    void SetPreviewMode(EPreviewMode NewMode);

    /**
     * 更新预览几何
     * @param Geometry 几何描述
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|Preview")
    void UpdatePreviewGeometry(const FVoxelGeometryDesc& Geometry);

    // === 2D UI管理 ===

    /**
     * 更新参数显示
     * @param Request 编辑请求
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|UI")
    void UpdateParameterDisplay(const FVoxelEditRequest& Request);

    /**
     * 更新对称模式按钮
     * @param bXSym X轴对称
     * @param bYSym Y轴对称
     * @param bZSym Z轴对称
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|UI")
    void UpdateSymmetryButton(bool bXSym, bool bYSym, bool bZSym);

    /**
     * 更新操作提示
     * @param Prompt 提示文本
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Visual|UI")
    void UpdateOperationPrompt(const FText& Prompt);

private:
    // === 持有的UI引用 ===

    /** PreviewActor */
    UPROPERTY(BlueprintReadOnly, Category = "Visual|UI")
    TWeakObjectPtr<AVoxelPreviewActor> PreviewActor;

    /** 其他3D UI */
    UPROPERTY(BlueprintReadOnly, Category = "Visual|UI")
    TArray<TWeakObjectPtr<AActor>> Other3DUIs;

    /** 2D UI Widget */
    UPROPERTY(BlueprintReadOnly, Category = "Visual|UI")
    TArray<UUserWidget*> UIWidgets;

    // === 访问StateComponent ===

    UFUNCTION()
    UVoxelControllerStateComponent* GetStateComponent();

    // === 映射编辑类型为预览模式 ===

    UFUNCTION()
    EPreviewMode MapEditModeToPreviewMode(EVoxelControllerBuildState EditMode);
};
```

---

### 3.5 UVoxelControllerHistoryComponent

#### 职责
- 管理撤销/重做栈
- 记录操作日志
- 统计信息

#### 不做
- 不处理业务逻辑
- 不检测输入
- 不渲染

---

## 四、组件交互设计

### 4.1 组件依赖关系

```
蓝图（射线检测，计算体素索引）
└─ 调用：InteractComponent->UpdateBatchEditPreview(VoxelIndex)

UVoxelControllerInteractComponent
├─ 存储：FirstPoint、SecondPoint（输入状态）
├─ 持有：StateComponent引用
├─ 持有：BatchEditManager引用
└─ 持有：HistoryComponent引用

UVoxelControllerVisualUIComponent
├─ 持有：PreviewActor引用
├─ 持有：Other3DUIs引用
├─ 持有：UIWidgets引用
└─ 访问：StateComponent（只读）
```

### 4.2 组件通信方式

#### 方式1：直接引用（推荐）

```cpp
// InteractComponent持有其他组件的引用
class UVoxelControllerInteractComponent
{
private:
    // 输入状态存储
    UPROPERTY()
    FIntVector FirstPoint;

    UPROPERTY()
    FIntVector SecondPoint;

    // 组件引用
    UPROPERTY()
    TObjectPtr<UVoxelControllerStateComponent> StateComponent;

    UPROPERTY()
    TObjectPtr<UVoxelControllerHistoryComponent> HistoryComponent;
};
```

#### 方式2：事件广播（可选）

```cpp
// StateComponent广播事件
class UVoxelControllerStateComponent
{
public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildStateChanged, EVoxelControllerBuildState, NewState);

    UPROPERTY(BlueprintAssignable, Category = "Voxel|State|Events")
    FOnBuildStateChanged OnBuildStateChanged;

    void SetBuildState(EVoxelControllerBuildState NewState)
    {
        BuildState = NewState;
        OnBuildStateChanged.Broadcast(NewState);
    }
};

// VisualUIComponent监听事件
class UVoxelControllerVisualUIComponent
{
    virtual void BeginPlay() override
    {
        Super::BeginPlay();

        if (StateComponent)
        {
            StateComponent->OnBuildStateChanged.AddDynamic(this, &ThisClass::OnBuildStateChanged);
        }
    }

    UFUNCTION()
    void OnBuildStateChanged(EVoxelControllerBuildState NewState)
    {
        // 更新UI显示
    }
};
```

---

## 五、Controller接口设计

### 5.1 核心接口

```cpp
/**
 * Voxel玩家控制器
 *
 * 职责：
 * - 协调各Component
 * - 管理组件生命周期
 * - 提供统一的外部接口
 * - **接收蓝图输入并转发到组件**
 *
 * 不做：
 * - 不直接处理业务逻辑
 * - **不检测输入（蓝图处理）**
 * - 不直接渲染
 *
 * ⚠️ 实现说明：
 * - **输入检测和射线检测在蓝图层实现**
 * - C++ 层提供 BlueprintCallable 供蓝图调用
 * - C++ 层提供组件访问接口供蓝图使用
 */
UCLASS()
class AVoxelPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    // === 组件访问接口 ===

    /**
     * 获取状态组件
     */
    UFUNCTION(BlueprintPure, Category = "Voxel|Components")
    UVoxelControllerStateComponent* GetStateComponent() const;

    /**
     * 获取交互组件
     */
    UFUNCTION(BlueprintPure, Category = "Voxel|Components")
    UVoxelControllerInteractComponent* GetInteractComponent() const;

    /**
     * 获取视觉UI组件
     */
    UFUNCTION(BlueprintPure, Category = "Voxel|Components")
    UVoxelControllerVisualUIComponent* GetVisualUIComponent() const;

    /**
     * 获取历史组件
     */
    UFUNCTION(BlueprintPure, Category = "Voxel|Components")
    UVoxelControllerHistoryComponent* GetHistoryComponent() const;

    // === 高层接口（委托给对应Component） ===

    /**
     * 切换工具（委托给InteractComponent）
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Controller")
    void SwitchTool(EVoxelControllerBuildState NewTool);

    /**
     * 取消编辑（委托给InteractComponent）
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Controller")
    void CancelEdit();

    /**
     * 更新UI（委托给VisualUIComponent）
     */
    UFUNCTION(BlueprintCallable, Category = "Voxel|Controller")
    void UpdateUI();
};
```

### 5.2 组件初始化

```cpp
void AVoxelPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 确保所有组件已创建
    if (!GetInteractComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("InteractComponent not found!"));
    }

    if (!GetStateComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("StateComponent not found!"));
    }

    if (!GetInteractComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("InteractComponent not found!"));
    }

    if (!GetVisualUIComponent())
    {
        UE_LOG(LogTemp, Error, TEXT("VisualUIComponent not found!"));
    }

    // 初始化VisualUIComponent的PreviewActor
    if (UVoxelControllerVisualUIComponent* VisualUI = GetVisualUIComponent())
    {
        VisualUI->InitializePreviewActor(this);
    }
}
```

---

## 六、设计决策

### 决策1：输入统一处理

**决策**：蓝图检测输入并计算体素索引，InteractComponent管理输入状态

**理由**：
- 输入检测逻辑集中在蓝图，易于调试
- 输入状态存储在InteractComponent，便于业务逻辑访问
- 简化架构，减少不必要的组件

### 决策2：业务与渲染分离

**决策**：InteractComponent处理业务，VisualUIComponent处理渲染

**理由**：
- 职责清晰
- 易于测试
- 易于扩展

### 决策3：单向数据流

**决策**：业务数据 → 渲染指令 → 渲染执行

**理由**：
- 避免循环依赖
- 数据流向清晰
- 易于追踪问题

---

## 七、性能优化设计

### 7.1 组件引用缓存

**优化**: 在 `BeginPlay` 中一次性缓存组件引用，避免频繁的 `FindComponentByClass` 调用。

**实现**:
```cpp
void UVoxelControllerInteractComponent::BeginPlay()
{
    Super::BeginPlay();

    // 缓存组件引用（在 BeginPlay 中一次性初始化）
    if (AActor* Owner = GetOwner())
    {
        CachedStateComponent = Owner->FindComponentByClass<UVoxelControllerStateComponent>();
        CachedVisualUIComponent = Owner->FindComponentByClass<UVoxelControllerVisualUIComponent>();
        CachedHistoryComponent = Owner->FindComponentByClass<UVoxelControllerHistoryComponent>();
    }
}
```

**优点**:
- 避免每帧查询组件
- 减少不必要的 `FindComponentByClass` 开销
- 提升运行时性能

### 7.2 分离存储架构

**优化**: 将区域信息和几何信息分离存储，避免数据冗余和不必要的广播。

**设计**:
```cpp
// 分离存储结构
struct FTemporaryAreaInfo
{
    FName AreaName;
    FGameplayTag AreaType;
    int32 ParentAreaId;
    EAreaStatus Status;
    bool bIsValid;
    // 注意：不包含 Coverage，Coverage 存储在 TmpGeometry.Box 中
};

struct FVoxelGeometryDesc
{
    EBatchShapeType ShapeType;
    FAreaCoverageFragment Box;  // ← Coverage 独立存储
    FRotator Rotation;
    int32 Thickness;
    bool bIsRegular;
    FVector Center;
};
```

**优点**:
- 避免数据冗余
- 实现细粒度更新
- 降低内存占用

### 7.3 细粒度事件广播

**优化**: 根据数据变更类型使用不同的事件，避免不必要的广播。

**事件分类**:
```cpp
// 几何变更事件（高频，单独广播）
FOnTmpEditRequestChanged OnTmpEditRequestChanged;

// 字段变更事件（中频，单独广播）
FOnAreaFieldChanged OnAreaFieldChanged;

// 完整修改数据事件（低频，使用）
FOnAreaModificationChanged OnAreaModificationChanged;
```

**使用场景**:
- **拖拽 Gizmo**: 仅触发 `OnTmpEditRequestChanged`
- **修改区域名称**: 仅触发 `OnAreaFieldChanged`
- **切换选中区域**: 触发 `OnAreaModificationChanged`

**优点**:
- 避免不必要的监听者响应
- 降低 CPU 开销
- 提升交互响应速度

### 7.4 动态组装模式

**优化**: `FVoxelEditRequest` 在访问时动态组装，而非冗余存储。

**实现**:
```cpp
FVoxelEditRequest UVoxelControllerStateComponent::GetCurrentEditRequest() const
{
    // 动态组装：从各个独立状态字段 + TmpGeometry 组装成完整的 FVoxelEditRequest
    FVoxelEditRequest Request;
    Request.BuildState = BuildState;
    Request.EditMode = EditState;
    Request.BlockType = InteractableDataInHand.ItemTag;
    Request.Geometry = TmpGeometry;  // ← 从 TmpGeometry 获取完整的几何信息

    return Request;
}
```

**优点**:
- 避免数据同步问题
- 降低内存占用
- 简化状态管理

---

## 八、蓝图层实现

### 8.1 蓝图职责概述

**BP_VoxelPlayerController** 继承自 AVoxelPlayerController，负责：

**输入检测和响应**：
- 检测 Enhanced Input 事件
- 执行射线检测获取世界位置
- 计算体素索引（调用 C++ 工具方法）
- 根据状态调用不同的 C++ 接口

**状态流转协调**：
- 监听建造状态变更事件
- 根据状态启用/禁用输入
- 协调编辑流程的各个阶段

**UI 更新**：
- 监听 C++ 状态事件
- 更新 UI Widget 显示
- 播放动画和特效
- 显示提示信息

### 8.2 蓝图需要实现的功能模块

#### 8.2.1 输入处理模块

**鼠标输入处理**：
- 点击事件：根据当前状态调用 SetFirstPoint 或 SetSecondPoint
- 保持事件：调用 UpdateSecondPoint 或 UpdateGizmoInteraction
- 释放事件：调用 ReleaseGizmoInteraction

**键盘输入处理**：
- Enter/Space：调用 CommitBatchEditPreview 确认编辑
- Esc：调用 CancelCurrentEdit 取消编辑
- Ctrl+Z：调用 HistoryComponent->Undo 撤销
- Ctrl+Y：调用 HistoryComponent->Redo 重做
- R：旋转预览（修改 TmpGeometry）
- Tab：切换形状类型
- T：切换厚度模式
- Delete：切换到删除模式

#### 8.2.2 射线检测模块

**基础射线检测**：
- GetMouseWorldPosition：获取鼠标指向的世界位置
- GetMouseVoxelIndex：将世界位置转换为体素索引（调用 C++ WorldToVoxelIndex）
- GetClickedActor：获取点击的 Actor（用于 Gizmo 检测）

**Gizmo 交互检测**：
- IsClickedGizmo：检测是否点击了 Gizmo
- IsClickedArea：检测是否点击了区域

#### 8.2.3 编辑操作协调模块

**底面定义阶段（Base 状态）**：
- OnBasePhaseClick：调用 SetFirstPoint
- OnBasePhaseHold：调用 UpdateSecondPoint
- OnBasePhaseRelease：调用 SetSecondPoint

**编辑阶段（Edit 状态）**：
- OnEditPhaseGizmoClick：调用 StartGizmoInteraction
- OnEditPhaseGizmoDrag：调用 UpdateGizmoInteraction
- OnEditPhaseGizmoRelease：调用 ReleaseGizmoInteraction

**确认/取消**：
- OnConfirmEdit：调用 CommitBatchEditPreview
- OnCancelEdit：调用 CancelCurrentEdit

#### 8.2.4 UI 更新模块

**状态变更监听**：
- OnBuildStateChanged：根据状态切换 UI 显示
  - Hidden：隐藏编辑 UI
  - Base：显示底面定义提示
  - Edit：显示编辑提示和 Gizmo
  - Confirm：显示确认动画

**预览信息监听**：
- OnPreviewInfoChanged：更新体素数量、编辑模式、方块类型显示

**历史变更监听**：
- OnHistoryChanged：更新撤销/重做按钮状态

### 8.3 蓝图与 C++ 的交互方式

**蓝图调用 C++**：
- 通过 BlueprintCallable 标记的方法
- 通过 BlueprintReadWrite 标记的属性
- 同步调用，等待返回结果

**C++ 通知蓝图**：
- 通过 BlueprintAssignable 标记的委托
- C++ 广播事件，蓝图监听
- 异步通知，不需要蓝图等待

**数据传递**：
- 简单类型：int、bool、enum 直接传递
- 复杂类型：通过 USTRUCT 定义的结构体传递
- 避免频繁的 C++ ↔ 蓝图调用

### 8.4 蓝图实施要点

**性能优化**：
- 避免每帧执行复杂逻辑
- 缓存常用计算结果
- 减少事件绑定数量
- 优化射线检测频率

**调试建议**：
- 使用 Print String 输出调试信息
- 使用 Debug Line 可视化射线
- 设置断点查看变量值
- 使用蓝图分析器查看执行时间

**常见问题**：
- 射线检测不准确：检查碰撞通道设置
- 状态不更新：检查 C++ 接口调用
- UI 不刷新：检查事件绑定

### 8.5 参考文档

详细的蓝图实施步骤和方法清单，参见：
- [编辑系统-蓝图实施指南](/VoxelDesign/04-设计计划/编辑系统-蓝图实施指南.md)

---

## 九、实现建议

### 9.1 实现顺序

1. **阶段1：核心框架**
   - 实现AVoxelPlayerController
   - 实现各Component的基础类
   - 实现Component间的引用关系

2. **阶段2：输入处理**
   - 蓝图实现射线检测和体素索引计算
   - InteractComponent实现输入状态存储（FirstPoint、SecondPoint）
   - 实现状态机逻辑（HIDDEN → BASE → EDIT/CONFIRM）

3. **阶段3：业务处理**
   - 实现StateComponent
   - 实现InteractComponent
   - 实现业务逻辑

4. **阶段4：渲染处理**
   - 实现VisualUIComponent
   - 实现PreviewActor
   - 实现渲染更新逻辑

### 9.2 测试建议

1. **组件单元测试**
   - 测试各Component的独立功能
   - 测试接口调用

2. **集成测试**
   - 测试Component间的交互
   - 测试数据流
   - 测试完整流程

---

## 十、附录

### 10.1 相关文档

- [预览系统-PreviewActor-架构设计](/VoxelDesign/03-架构设计/预览系统-PreviewActor-架构设计.md) - PreviewActor详细设计
- [预览系统-状态管理-系统设计](/VoxelDesign/02-系统设计/预览系统-状态管理-系统设计.md) - 状态管理设计
- [预览系统-数据流-系统设计](/VoxelDesign/02-系统设计/预览系统-数据流-系统设计.md) - 数据流设计
- [编辑系统-C++与蓝图职责划分-系统设计](/VoxelDesign/02-系统设计/编辑系统-C++与蓝图职责划分-系统设计.md) - C++与蓝图职责划分
- [编辑系统-C++与蓝图职责划分-架构设计](/VoxelDesign/03-架构设计/编辑系统-C++与蓝图职责划分-架构设计.md) - C++与蓝图架构设计
- [编辑系统-蓝图实施指南](/VoxelDesign/04-设计计划/编辑系统-蓝图实施指南.md) - 蓝图实施详细步骤

### 10.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |
| v1.1 | 2026-01-19 | 根据实际代码更新架构设计 |
| | | - 更新 StateComponent：添加分离存储架构、细粒度事件系统、动态组装模式 |
| | | - 更新 InteractComponent：扩展职责（道具、区域、历史、Gizmo操作）、添加组件引用缓存 |
| | | - 更新 VisualUIComponent：明确Gizmo渲染管理职责、添加事件绑定说明 |
| | | - 新增"性能优化设计"章节：组件引用缓存、分离存储、细粒度事件、动态组装 |
| v1.2 | 2026-01-19 | 添加蓝图层实现说明 |
| | | - 新增"八、蓝图层实现"章节 |
| | | - 说明蓝图职责和需要实现的功能模块 |
| | | - 添加蓝图与 C++ 交互方式说明 |
| | | - 添加蓝图实施要点和参考文档 |

---

**文档状态**：✅ 已更新（v1.1 - 2026-01-19）
