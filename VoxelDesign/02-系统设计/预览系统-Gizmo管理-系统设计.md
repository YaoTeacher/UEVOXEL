# Gizmo 管理系统设计

> **文档目的**：定义VisualUIComponent中的Gizmo管理架构、接口设计和输入处理流程
> **创建日期**：2026-01-18
> **版本**：v2.0
> **状态**：已实现

---

## 一、概述

### 1.1 Gizmo定位

**Gizmo**是VisualUIComponent管理的3D UI交互元素，用于建造预览的交互操作。

**核心定位**：
- 由VisualUIComponent统一创建、管理和销毁
- 作为用户交互的输入接口（点击、拖动）
- 位置和可见性由PreviewActor的包围盒驱动
- 为未来家具拖拽功能预留

### 1.2 设计原则

1. **统一管理**：所有Gizmo由VisualUIComponent统一管理
2. **职责分离**：PreviewActor专注渲染，Gizmo负责交互
3. **蓝图输入**：输入事件由蓝图处理，C++提供接口
4. **数据驱动**：Gizmo位置和可见性由PreviewActor的包围盒数据驱动

---

## 二、Gizmo分类

### 2.1 三种Gizmo类型

**12个Gizmo分为3组**：

#### TranslateGizmos（平移Gizmo）
- **数量**：3个（X/Y/Z轴）
- **命名**：
  - `XTranslateGizmo`：沿X轴平移
  - `YTranslateGizmo`：沿Y轴平移
  - `ZTranslateGizmo`：沿Z轴平移
- **用途**：沿轴向移动预览位置
- **Mesh**：箭头模型（`TranslateGizmoMesh`）

#### RotateGizmos（旋转Gizmo）
- **数量**：3个（X/Y/Z轴）
- **命名**：
  - `RotateGizmos[0]`：绕X轴旋转
  - `RotateGizmos[1]`：绕Y轴旋转
  - `RotateGizmos[2]`：绕Z轴旋转
- **用途**：旋转预览对象
- **Mesh**：圆环模型（`RotateGizmoMesh`）

#### FaceGizmos（面Gizmo）
- **数量**：6个（立方体6个面）
- **命名**：
  - `Face_MinX`：-X面
  - `Face_MaxX`：+X面
  - `Face_MinY`：-Y面
  - `Face_MaxY`：+Y面
  - `Face_MinZ`：-Z面
  - `Face_MaxZ`：+Z面
- **用途**：调整包围盒形状（推拉面）
- **Mesh**：平面模型（`FaceGizmoMesh`）

#### HeightCollisionVisual（高度碰撞可视化）

- **类型**：AActor（由`HeightCollisionClass`配置）
- **命名**：`HeightCollisionVisual`
- **用途**：显示高度碰撞检测的可视化平面
- **显示条件**：EDIT 状态下显示
- **旋转规则**：
  - 垂直于 XY 平面（Pitch = 0）
  - 朝向摄像头方向（根据 Yaw 旋转）
  - 旋转公式：`FRotator(0, Yaw, 0)`

### 2.2 Gizmo属性配置

| 属性 | 类型 | 用途 |
|------|------|------|
| **TranslateGizmoClass** | TSubclassOf<AVoxel3DUIActor> | Gizmo类引用 |
| **TranslateGizmoMesh** | UStaticMesh* | 平移Gizmo网格（箭头） |
| **RotateGizmoMesh** | UStaticMesh* | 旋转Gizmo网格（圆环） |
| **FaceGizmoMesh** | UStaticMesh* | 面Gizmo网格（平面） |
| **GizmoHandleSize** | float | Gizmo缩放大小 |

---

## 三、类设计

### 3.1 VisualUIComponent中的Gizmo成员

#### Gizmo实例变量

```cpp
class UVoxelControllerVisualUIComponent
{
private:
    // 平移Gizmo（3个轴）
    UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
    TObjectPtr<AVoxel3DUIActor> XTranslateGizmo;

    UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
    TObjectPtr<AVoxel3DUIActor> YTranslateGizmo;

    UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
    TObjectPtr<AVoxel3DUIActor> ZTranslateGizmo;

    // 旋转Gizmo（3个轴）
    UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
    TArray<TObjectPtr<AVoxel3DUIActor>> RotateGizmos;

    // 面Gizmo（6个面）
    UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
    TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;
};
```

#### Gizmo状态

```cpp
// 当前激活的Gizmo类型
UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
EInteractiveGizmoType ActiveGizmoType = EInteractiveGizmoType::None;

// 当前选中的Gizmo手柄
UPROPERTY(BlueprintReadOnly, Category = "Controller|Visual|Gizmo")
EGizmoHandleID SelectedGizmoHandle = EGizmoHandleID::None;
```

### 3.2 初始化接口

#### InitTranslateGizmos

**签名**：
```cpp
void InitTranslateGizmos()
```

**调用时机**：BeginPlay

**职责**：
- 创建3个TranslateGizmo
- 设置Mesh（使用`TranslateGizmoMesh`）
- 设置Direction（Forward/Right/Up）
- 设置OwningController
- 初始隐藏

#### InitRotateGizmos

**签名**：
```cpp
void InitRotateGizmos()
```

**调用时机**：BeginPlay

**职责**：
- 创建3个RotateGizmo
- 设置Mesh（使用`RotateGizmoMesh`）
- 设置旋转方向（使每个轴的ROTATOR垂直于对应轴）
- 设置初始缩放为 1.0
- 设置OwningController
- 初始隐藏

**ROTATOR 旋转设置**：
```cpp
// 模型要求：XY平面网格（法线为+Z方向）
// 旋转后使每个轴的ROTATOR垂直于对应轴
FRotator AxisRotations[3] = {
    FRotator(0, 0, 0),       // X 轴：垂直于 X 轴（在 YZ 平面）
    FRotator(0, 90, 90),     // Y 轴：垂直于 Y 轴（在 XZ 平面）
    FRotator(90, 0, 0)       // Z 轴：垂直于 Z 轴（在 XY 平面）
};
```

**缩放说明**：
- 初始化时缩放设为 1.0
- 运行时不动态调整缩放（ROTATOR使用统一缩放）

#### InitFaceGizmos

**签名**：
```cpp
void InitFaceGizmos()
```

**调用时机**：BeginPlay

**职责**：
- 创建6个FaceGizmo
- 设置Mesh（使用`FaceGizmoMesh`）
- 设置旋转（使Gizmo与对应面平行）
- 设置初始缩放为 `GizmoHandleSize / 100.0f`
- 设置OwningController
- 初始隐藏

**Face Gizmo旋转设置**：
```cpp
模型要求：100×100的XY平面网格（法线为+Z方向）

初始旋转（使Gizmo与对应面平行）：
├─ MinX (-X面): Pitch=90°
├─ MaxX (+X面): Pitch=-90°
├─ MinY (-Y面): Roll=90°
├─ MaxY (+Y面): Roll=-90°
├─ MinZ (-Z面): Roll=180°
└─ MaxZ (+Z面): 无旋转
```

**缩放说明**：
- 初始化时缩放设为 `GizmoHandleSize / 100.0f`
- 运行时不动态调整缩放（FACE使用固定初始缩放）

### 3.3 显示控制接口

#### TranslateGizmo控制

**签名**：
```cpp
void ShowTranslateGizmo(const FVector& Center)
void HideTranslateGizmo()
```

**参数**：
- Center - Gizmo中心位置（世界坐标）

**用途**：控制TranslateGizmo的显示和隐藏

#### RotateGizmo控制

**签名**：
```cpp
void ShowRotateGizmo(const FVector& Center)
void HideRotateGizmo()
```

**参数**：
- Center - Gizmo中心位置（世界坐标）

**用途**：控制RotateGizmo的显示和隐藏

#### 统一控制

**签名**：
```cpp
void HideAllGizmos()
```

**用途**：隐藏所有Gizmo

---

## 四、输入处理

### 4.1 输入处理架构

**方式**：蓝图事件处理 + C++ 接口

**设计原则**：
- 蓝图负责输入检测和偏移量计算
- C++提供业务逻辑接口
- 通过`InteractComponent::MoveArea()`连接蓝图和C++

### 4.2 完整输入流程

```
用户点击/拖动Gizmo
    ↓
蓝图捕获输入事件
    ↓
蓝图计算Offset（拖动距离）
    ↓
蓝图调用 C++ 接口：InteractComponent->MoveArea(Offset, Gizmo)
    ↓
C++ 更新 StateComponent
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent 接收广播
    ↓
VisualUIComponent 更新：
  ├─ PreviewActor 渲染（调用 UpdateFromEditRequest）
  └─ Gizmo 位置（调用 UpdateTranslateGizmosPosition）
```

### 4.3 关键C++接口

#### MoveArea（InteractComponent）

**签名**：
```cpp
void MoveArea(FVector Offset, AVoxel3DUIActor* Input)
```

**参数**：
- Offset - 拖动偏移量（世界坐标）
- Input - 被拖动的Gizmo

**调用者**：蓝图

**用途**：处理Gizmo拖动，更新StateComponent

**实现逻辑**：
```cpp
void UVoxelControllerInteractComponent::MoveArea(FVector Offset, AVoxel3DUIActor* Input)
{
    AVoxelPlayerController* PC = GetOwner<AVoxelPlayerController>();
    if (!IsValid(PC)) return;

    int b = PC->GetInstance()->WorldConfig.BlockLength;
    Offset = Offset / b;  // 转换为体素索引

    // 识别拖动的是哪个轴
    if (Input == PC->VisualComp->XTranslateGizmo)
    {
        PC->StateComp->MoveTmpAreaCoverage(FIntVector(Offset.X, 0, 0));
    }
    else if (Input == PC->VisualComp->YTranslateGizmo)
    {
        PC->StateComp->MoveTmpAreaCoverage(FIntVector(0, Offset.Y, 0));
    }
    else if (Input == PC->VisualComp->ZTranslateGizmo)
    {
        PC->StateComp->MoveTmpAreaCoverage(FIntVector(0, 0, Offset.Z));
    }
}
```

---

## 五、位置同步

### 5.1 位置更新时机

**触发时机**：
- StateComponent广播`OnTmpEditRequestChanged(Request)`
- VisualUIComponent接收广播后调用`UpdateBatchEditPreview()`

### 5.2 位置更新方法

#### UpdateTranslateGizmosPosition

**签名**：
```cpp
void UpdateTranslateGizmosPosition(FVector Center)
```

**参数**：
- Center - Gizmo中心位置（世界坐标）

**用途**：同步TranslateGizmo位置到预览中心

**调用时机**：
- `UpdateSingleVoxelPreview()` - 单体素预览
- `UpdateBatchEditPreview()` - 批量编辑预览
- `HandleTmpEditRequestChanged()` - 编辑请求变化

#### UpdateRotateGizmosPosition

**签名**：
```cpp
void UpdateRotateGizmosPosition(FVector Center)
```

**参数**：
- Center - Gizmo中心位置（世界坐标）

**用途**：同步RotateGizmo位置到预览中心

**调用时机**：
- `UpdateAllGizmosPosition()` - 统一更新所有Gizmo位置

**说明**：
- 只更新位置，不动态调整缩放
- ROTATOR 使用统一的初始缩放（1.0）

#### UpdateFaceGizmosPosition

**签名**：
```cpp
void UpdateFaceGizmosPosition(const FBox& Bounds)
```

**参数**：
- Bounds - 预览包围盒（世界坐标）

**用途**：同步FaceGizmo位置到包围盒的各个面

**调用时机**：
- `UpdateAllGizmosPosition()` - 统一更新所有Gizmo位置

**说明**：
- 更新每个FaceGizmo到对应面的中心位置
- 不动态调整缩放（使用固定初始缩放）

### 5.3 数据流

```
StateComponent.Geometry 更新
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
    ↓
FBox WorldBounds = Request.Geometry.GetBound(BlockLength)
    ↓
FVector Center = WorldBounds.GetCenter()
    ↓
UpdateTranslateGizmosPosition(Center)
    ↓
X/Y/ZTranslateGizmo->SetActorLocation(Center)
```

---

## 六、设计决策

### 6.1 为什么Gizmo由VisualUIComponent管理？

**决策**：所有Gizmo由VisualUIComponent统一管理，而不是放在PreviewActor中

**理由**：
1. **职责分离**：
   - PreviewActor专注纯渲染
   - VisualUIComponent负责UI交互元素

2. **语义清晰**：
   - Gizmo是UI交互元素，不是渲染对象
   - VisualUIComponent是"视觉UI"组件，Gizmo属于UI

3. **未来扩展**：
   - 为家具拖拽功能预留
   - Gizmo可用于拖拽其他可交互对象

4. **同步简单**：
   - VisualUIComponent同时持有PreviewActor和Gizmo
   - 可以方便地根据PreviewActor的包围盒同步Gizmo位置

### 6.2 为什么使用蓝图+ C++ 混合输入处理？

**决策**：蓝图捕获输入事件，C++提供业务逻辑接口

**理由**：
1. **蓝图优势**：
   - 输入事件处理更直观
   - 可视化调试方便
   - 热重载支持

2. **C++ 优势**：
   - 业务逻辑性能更好
   - 类型安全
   - 代码维护性好

3. **职责分离**：
   - 蓝图：输入检测、偏移量计算
   - C++：状态更新、业务逻辑

### 6.3 为什么 Gizmo 使用固定缩放而非动态缩放？

**决策**：ROTATOR 和 FACE 使用固定初始缩放，运行时不动态调整

**理由**：
1. **ROTATOR（旋转 Gizmo）**：
   - 初始化缩放设为 1.0（统一缩放）
   - 运行时只更新位置，不调整缩放
   - 简化实现，避免缩放计算的复杂性

2. **FACE（面 Gizmo）**：
   - 初始化缩放设为 `GizmoHandleSize / 100.0f`
   - 运行时不根据包围盒大小动态调整
   - 保持 Gizmo 大小一致，避免视觉干扰

3. **缩放基准**：
   - 所有 Gizmo 的缩放都基于 100 的基准包围盒大小
   - `GizmoHandleSize` 配置项控制 Gizmo 的基础大小

---

## 七、实现建议

### 7.1 文件组织

```
Source/Voxel/
├── Public/Core/ControllerComponent/
│   └── VoxelControllerVisualUIComponent.h
└── Private/Core/ControllerComponent/
    └── VoxelControllerVisualUIComponent.cpp
```

### 7.2 实现优先级

```
阶段1：Gizmo初始化
├─ InitTranslateGizmos()
├─ InitRotateGizmos()
└─ InitFaceGizmos()

阶段2：显示控制
├─ ShowTranslateGizmo()
├─ HideTranslateGizmo()
├─ ShowRotateGizmo()
├─ HideRotateGizmo()
└─ HideAllGizmos()

阶段3：位置同步
└─ UpdateTranslateGizmosPosition()

阶段4：蓝图集成
└─ 实现蓝图输入事件
    └─ 调用 InteractComponent->MoveArea()
```

---

## 八、附录

### 8.1 相关文档

- `预览系统-PreviewActor-架构设计.md` - PreviewActor设计（专注渲染）
- `预览系统-架构设计.md` - 预览系统整体架构
- `编辑系统-Controller-架构设计.md` - Controller整体架构
- `05-设计决策记录/预览系统-Gizmo移至VisualUIComponent-决策记录.md` - 决策记录

### 8.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本（PreviewActor管理Gizmo） |
| v2.0 | 2026-01-18 | **重大重构**：Gizmo移至VisualUIComponent管理 |
| v2.1 | 2026-01-18 | **细化实现**：明确 Gizmo 旋转角度、缩放策略和更新接口 |
  - 更新 ROTATOR 旋转角度配置
  - 明确 ROTATOR 使用固定缩放（1.0）
  - 明确 FACE 使用固定初始缩放（GizmoHandleSize/100）
  - 移除动态缩放逻辑，简化实现
  - 添加 HEIGHTCOLLI 旋转说明（Pitch=0，垂直于XY平面）

---

**文档状态**：✅ 已实现
