# Gizmo 交互逻辑 - 蓝图实现指南

> **文档目的**：说明如何在 CONTROLLER 蓝图中实现 Gizmo 的交互逻辑（旋转、拖动、面推拉）
> **创建日期**：2026-01-18
> **版本**：v1.0
> **状态**：实现指南

---

## 一、概述

### 1.1 交互流程

```
蓝图输入捕获 → ModifyGizmo 调用 → 内部路由 → 状态更新 → 视觉更新
```

**核心设计原则**：
- **蓝图负责**：输入事件捕获、Transform 计算、调用 C++ 接口
- **C++ 负责**：业务逻辑、状态更新、自动触发视觉更新
- **数据流**：蓝图 → InteractComponent → StateComponent → VisualUIComponent → PreviewActor

### 1.2 统一入口

所有 Gizmo 交互都通过同一个 C++ 接口：

```cpp
// 位置：VoxelControllerInteractComponent.h:206-207
void ModifyGizmo(FTransform Transform, AVoxel3DUIActor* Input);
```

**参数说明**：
- **Transform**：Gizmo 的当前变换（包含位移和旋转）
- **Input**：被拖动的 Gizmo 指针（用于识别是哪个 Gizmo）

---

## 二、Voxel3DUIActor 事件委托

### 2.1 OnDragged 委托

```cpp
// 位置：Voxel3DUIActor.h:12
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIActorDragged, AVoxel3DUIActor*, DraggedActor, FVector, DragDelta);
```

**参数说明**：
- **DraggedActor**：被拖动的 Gizmo Actor 指针
- **DragDelta**：拖动偏移量（世界坐标）

**使用位置**：Voxel3DUIActor.OnDragged

### 2.2 委托绑定

在 CONTROLLER 蓝图中，需要为每个 Gizmo 绑定 OnDragged 事件：

```
事件图表：
├─ BeginPlay
│  └─ 为所有 Gizmo 绑定 OnDragged 事件
│     ├─ XTranslateGizmo.OnDragged → HandleGizmoDragged
│     ├─ YTranslateGizmo.OnDragged → HandleGizmoDragged
│     ├─ ZTranslateGizmo.OnDragged → HandleGizmoDragged
│     ├─ RotateGizmos[0].OnDragged → HandleGizmoDragged
│     ├─ RotateGizmos[1].OnDragged → HandleGizmoDragged
│     ├─ RotateGizmos[2].OnDragged → HandleGizmoDragged
│     └─ FaceGizmos[0-5].OnDragged → HandleGizmoDragged
```

---

## 三、蓝图事件处理

### 3.1 HandleGizmoDragged 事件函数

创建一个自定义事件 `HandleGizmoDragged`：

```
事件签名：
├─ 输入参数：
│  ├─ DraggedActor: AVoxel3DUIActor* (被拖动的 Gizmo)
│  └─ DragDelta: FVector (拖动偏移，世界坐标)
```

### 3.2 事件处理逻辑

```
HandleGizmoDragged(DraggedActor, DragDelta)
│
├─ 步骤1：获取 Gizmo 当前 Transform
│  └─ DraggedActor.GetActorTransform()
│
├─ 步骤2：调用 C++ 接口
│  └─ InteractComponent.ModifyGizmo(Transform, DraggedActor)
│
└─ 步骤3：C++ 自动处理后续逻辑
   ├─ 识别 Gizmo 类型（Translate/Rotate/Face）
   ├─ 提取对应的 Transform 分量
   ├─ 更新 StateComponent
   └─ 自动触发 VisualUIComponent 更新
```

---

## 四、C++ 内部路由逻辑

### 4.1 ModifyGizmo 实现逻辑

```cpp
// 位置：VoxelControllerInteractComponent.cpp:418-530
void UVoxelControllerInteractComponent::ModifyGizmo(FTransform Transform, AVoxel3DUIActor* Input)
{
    // 1. 获取 BlockLength
    float BlockLength = PC->GetInstance()->WorldConfig.BlockLength;

    // 2. 识别 Gizmo 类型并路由到对应逻辑

    // ==================== Translate Gizmos ====================
    if (Input == PC->VisualComp->XTranslateGizmo)
    {
        FVector Translation = Transform.GetTranslation();
        FIntVector Offset(FMath::RoundToInt(Translation.X / BlockLength), 0, 0);
        PC->StateComp->MoveTmpAreaCoverage(Offset);  // ← 更新状态
        return;  // ← 自动触发 VisualUIComponent 更新
    }

    if (Input == PC->VisualComp->YTranslateGizmo)
    {
        FVector Translation = Transform.GetTranslation();
        FIntVector Offset(0, FMath::RoundToInt(Translation.Y / BlockLength), 0);
        PC->StateComp->MoveTmpAreaCoverage(Offset);
        return;
    }

    if (Input == PC->VisualComp->ZTranslateGizmo)
    {
        FVector Translation = Transform.GetTranslation();
        FIntVector Offset(0, 0, FMath::RoundToInt(Translation.Z / BlockLength));
        PC->StateComp->MoveTmpAreaCoverage(Offset);
        return;
    }

    // ==================== Rotate Gizmos ====================
    for (int32 i = 0; i < PC->VisualComp->RotateGizmos.Num(); i++)
    {
        if (Input == PC->VisualComp->RotateGizmos[i])
        {
            // RotateGizmos[0] = X轴, [1] = Y轴, [2] = Z轴
            FRotator Rotation = Transform.GetRotation().Rotator();
            PC->StateComp->SetTmpGeometryRotation(Rotation);  // ← 更新状态
            return;  // ← 自动触发 VisualUIComponent 更新
        }
    }

    // ==================== Face Gizmos ====================
    for (int32 i = 0; i < PC->VisualComp->FaceGizmos.Num(); i++)
    {
        if (Input == PC->VisualComp->FaceGizmos[i])
        {
            FBox CurrentBox = PC->StateComp->GetTmpGeometry().Box.BoundingBox();
            FVector Min = CurrentBox.Min;
            FVector Max = CurrentBox.Max;
            FVector Translation = Transform.GetTranslation();

            // 根据拖动的面计算新的包围盒
            switch (i)
            {
            case 0: // MinX (-X面)
                Min.X += FMath::RoundToInt(Translation.X / BlockLength);
                break;
            case 1: // MaxX (+X面)
                Max.X += FMath::RoundToInt(Translation.X / BlockLength);
                break;
            case 2: // MinY (-Y面)
                Min.Y += FMath::RoundToInt(Translation.Y / BlockLength);
                break;
            case 3: // MaxY (+Y面)
                Max.Y += FMath::RoundToInt(Translation.Y / BlockLength);
                break;
            case 4: // MinZ (-Z面)
                Min.Z += FMath::RoundToInt(Translation.Z / BlockLength);
                break;
            case 5: // MaxZ (+Z面)
                Max.Z += FMath::RoundToInt(Translation.Z / BlockLength);
                break;
            }

            // 创建新的包围盒并设置
            FAreaCoverageFragment NewBox = FAreaCoverageFragment(
                FIntVector(FMath::RoundToInt(Min.X), FMath::RoundToInt(Min.Y), FMath::RoundToInt(Min.Z)),
                FIntVector(FMath::RoundToInt(Max.X), FMath::RoundToInt(Max.Y), FMath::RoundToInt(Max.Z))
            );
            PC->StateComp->SetTmpAreaCoverage(NewBox);  // ← 更新状态
            return;  // ← 自动触发 VisualUIComponent 更新
        }
    }
}
```

### 4.2 自动更新流程

```
StateComponent 更新（MoveTmpAreaCoverage/SetTmpGeometryRotation/SetTmpAreaCoverage）
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
    ↓
PreviewActor.UpdateFromEditRequest(Request, BlockLength)
    ↓
Gizmo 位置自动同步（UpdateAllGizmosPosition）
```

---

## 五、蓝图节点设置

### 5.1 BeginPlay 事件绑定

```
节点序列：
├─ Event BeginPlay
├─ For Loop (遍历 TranslateGizmos)
│  ├─ Get VisualComp.XTranslateGizmo
│  ├─ Bind Event to OnDragged
│  └─ Target: HandleGizmoDragged
├─ For Loop (遍历 RotateGizmos)
│  ├─ Get VisualComp.RotateGizmos
│  ├─ Bind Event to OnDragged
│  └─ Target: HandleGizmoDragged
└─ For Loop (遍历 FaceGizmos)
   ├─ Get VisualComp.FaceGizmos
   ├─ Bind Event to OnDragged
   └─ Target: HandleGizmoDragged
```

### 5.2 HandleGizmoDragged 实现

```
自定义事件：HandleGizmoDragged
├─ 输入：
│  ├─ DraggedActor: AVoxel3DUIActor* Object Reference
│  └─ DragDelta: Vector
├─ 逻辑：
│  ├─ Get DraggedActor Actor Transform
│  ├─ Get InteractComponent
│  ├─ Call ModifyGizmo(Transform, DraggedActor)
│  └─ Return
```

---

## 六、参数传递说明

### 6.1 Transform 参数

**用途**：Gizmo 的当前世界变换

**获取方式**：
```
蓝图节点：
├─ DraggedActor (AVoxel3DUIActor*)
└─ GetActorTransform() → Transform
```

**包含内容**：
- **Translation**：位移（用于 TranslateGizmo 和 FaceGizmo）
- **Rotation**：旋转（用于 RotateGizmo）
- **Scale**：缩放（当前未使用）

### 6.2 Input 参数

**用途**：识别是哪个 Gizmo 被拖动

**传递方式**：直接传递 OnDragged 委托的 DraggedActor 参数

**识别逻辑**：C++ 通过指针比较识别 Gizmo 类型
```cpp
if (Input == PC->VisualComp->XTranslateGizmo)  // X轴平移
if (Input == PC->VisualComp->RotateGizmos[0])  // X轴旋转
if (Input == PC->VisualComp->FaceGizmos[0])    // -X面
```

---

## 七、Gizmo 类型识别

### 7.1 Translate Gizmo（3个）

| C++ 指针 | 轴向 | 提取分量 | 调用方法 |
|---------|------|---------|---------|
| `XTranslateGizmo` | X轴 | Translation.X | `MoveTmpAreaCoverage(FIntVector(Offset.X, 0, 0))` |
| `YTranslateGizmo` | Y轴 | Translation.Y | `MoveTmpAreaCoverage(FIntVector(0, Offset.Y, 0))` |
| `ZTranslateGizmo` | Z轴 | Translation.Z | `MoveTmpAreaCoverage(FIntVector(0, 0, Offset.Z))` |

### 7.2 Rotate Gizmo（3个）

| C++ 数组索引 | 轴向 | 提取分量 | 调用方法 |
|------------|------|---------|---------|
| `RotateGizmos[0]` | X轴 | Rotation | `SetTmpGeometryRotation(Rotation)` |
| `RotateGizmos[1]` | Y轴 | Rotation | `SetTmpGeometryRotation(Rotation)` |
| `RotateGizmos[2]` | Z轴 | Rotation | `SetTmpGeometryRotation(Rotation)` |

### 7.3 Face Gizmo（6个）

| C++ 数组索引 | 面 | 提取分量 | 修改包围盒 |
|------------|---|---------|----------|
| `FaceGizmos[0]` | -X面 (MinX) | Translation.X | Min.X += Offset |
| `FaceGizmos[1]` | +X面 (MaxX) | Translation.X | Max.X += Offset |
| `FaceGizmos[2]` | -Y面 (MinY) | Translation.Y | Min.Y += Offset |
| `FaceGizmos[3]` | +Y面 (MaxY) | Translation.Y | Max.Y += Offset |
| `FaceGizmos[4]` | -Z面 (MinZ) | Translation.Z | Min.Z += Offset |
| `FaceGizmos[5]` | +Z面 (MaxZ) | Translation.Z | Max.Z += Offset |

---

## 八、完整数据流

### 8.1 Translate Gizmo 拖动

```
用户拖动 XTranslateGizmo
    ↓
OnDragged(DraggedActor=XTranslateGizmo, DragDelta)
    ↓
HandleGizmoDragged(XTranslateGizmo, DragDelta)
    ↓
GetActorTransform(Transform)
    ↓
InteractComponent.ModifyGizmo(Transform, XTranslateGizmo)
    ↓
C++: 提取 Translation.X
C++: 转换为体素索引: Offset = FMath::RoundToInt(Translation.X / BlockLength)
C++: StateComponent.MoveTmpAreaCoverage(FIntVector(Offset, 0, 0))
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
    ↓
PreviewActor.UpdateFromEditRequest(Request, BlockLength)
    ↓
Gizmo 位置自动同步（UpdateTranslateGizmosPosition）
```

### 8.2 Rotate Gizmo 拖动

```
用户拖动 RotateGizmos[0] (X轴旋转环)
    ↓
OnDragged(DraggedActor=RotateGizmos[0], DragDelta)
    ↓
HandleGizmoDragged(RotateGizmos[0], DragDelta)
    ↓
GetActorTransform(Transform)
    ↓
InteractComponent.ModifyGizmo(Transform, RotateGizmos[0])
    ↓
C++: 识别为 RotateGizmos[0] (X轴)
C++: 提取 Rotation
C++: StateComponent.SetTmpGeometryRotation(Rotation)
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
    ↓
PreviewActor.UpdateFromEditRequest(Request, BlockLength)
    ↓
Gizmo 位置自动同步（UpdateRotateGizmosPosition）
```

### 8.3 Face Gizmo 拖动

```
用户拖动 FaceGizmos[1] (+X面)
    ↓
OnDragged(DraggedActor=FaceGizmos[1], DragDelta)
    ↓
HandleGizmoDragged(FaceGizmos[1], DragDelta)
    ↓
GetActorTransform(Transform)
    ↓
InteractComponent.ModifyGizmo(Transform, FaceGizmos[1])
    ↓
C++: 识别为 FaceGizmos[1] (+X面)
C++: 提取 Translation.X
C++: 转换为体素索引: Offset = FMath::RoundToInt(Translation.X / BlockLength)
C++: 修改包围盒: NewBox.Max.X += Offset
C++: StateComponent.SetTmpAreaCoverage(NewBox)
    ↓
StateComponent 广播 OnTmpEditRequestChanged(Request)
    ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
    ↓
PreviewActor.UpdateFromEditRequest(Request, BlockLength)
    ↓
Gizmo 位置和缩放自动同步（UpdateFaceGizmosPosition）
```

---

## 九、蓝图实现示例

### 9.1 事件图表伪代码

```
// === BeginPlay ===
Event BeginPlay
│
├─ Get Visual Component
├─ Get Interact Component
│
├─ // 绑定 Translate Gizmos
├─ VisualComp.XTranslateGizmo.OnDragged.Add(HandleGizmoDragged)
├─ VisualComp.YTranslateGizmo.OnDragged.Add(HandleGizmoDragged)
├─ VisualComp.ZTranslateGizmo.OnDragged.Add(HandleGizmoDragged)
│
├─ // 绑定 Rotate Gizmos
├─ For Each Element in VisualComp.RotateGizmos
│  └─ Element.OnDragged.Add(HandleGizmoDragged)
│
└─ // 绑定 Face Gizmos
   └─ For Each Element in VisualComp.FaceGizmos
      └─ Element.OnDragged.Add(HandleGizmoDragged)


// === HandleGizmoDragged ===
Custom Event HandleGizmoDragged(DraggedActor, DragDelta)
│  输入:
│  ├─ DraggedActor: AVoxel3DUIActor Object Reference
│  └─ DragDelta: Vector
│
├─ // 获取 Gizmo 当前 Transform
├─ Variable: GizmoTransform = DraggedActor.GetActorTransform()
│
├─ // 调用 C++ 接口
├─ InteractComponent.ModifyGizmo(
│     Transform = GizmoTransform,
│     Input = DraggedActor
│  )
│
└─ // C++ 自动处理后续逻辑（状态更新、视觉更新）
```

### 9.2 蓝图节点清单

| 节点类型 | 节点名称 | 用途 |
|---------|---------|------|
| Event | BeginPlay | 初始化事件绑定 |
| Custom Event | HandleGizmoDragged | 处理 Gizmo 拖动 |
| Get Actor Transform | DraggedActor.GetActorTransform | 获取 Gizmo 当前变换 |
| Call Function | InteractComponent.ModifyGizmo | 调用 C++ 接口 |
| Bind Event | OnDragged.Add | 绑定委托事件 |
| For Loop | 遍历数组 | 批量绑定 RotateGizmos/FaceGizmos |

---

## 十、注意事项

### 10.1 坐标空间转换

- **DragDelta**：世界坐标（单位：厘米）
- **体素索引**：除以 BlockLength 后四舍五入
  ```cpp
  int32 Offset = FMath::RoundToInt(Translation.X / BlockLength);
  ```

### 10.2 指针有效性检查

C++ 接口内部已包含有效性检查：
```cpp
if (!Input) return;
if (!IsValid(PC) || !IsValid(PC->VisualComp) || !IsValid(PC->StateComp)) return;
```

蓝图无需额外检查。

### 10.3 自动更新机制

**重要**：蓝图只需调用 `ModifyGizmo`，后续所有更新都由 C++ 自动处理：
- StateComponent 更新
- 委托广播
- VisualUIComponent 接收
- PreviewActor 渲染更新
- Gizmo 位置同步

**蓝图不需要**：
- ❌ 手动更新 Gizmo 位置
- ❌ 手动调用 PreviewActor 更新
- ❌ 手动处理委托广播

### 10.4 性能考虑

- OnDragged 事件可能每帧触发多次
- C++ 内部使用 `VeryVerbose` 日志级别，避免性能影响
- 如果需要限制更新频率，可在蓝图端添加节流逻辑

---

## 十一、调试技巧

### 11.1 日志输出

C++ 内部已包含详细日志：
```cpp
UE_LOG(LogTemp, VeryVerbose, TEXT("ModifyGizmo: X Axis, Offset: %s"), *Offset.ToString());
UE_LOG(LogTemp, VeryVerbose, TEXT("ModifyGizmo: Rotate %d, Delta: %s"), i, *Rotation.ToString());
UE_LOG(LogTemp, VeryVerbose, TEXT("ModifyGizmo: Face MinX, Delta: %s"), *Translation.ToString());
```

启用 `VeryVerbose` 日志级别查看详细信息。

### 11.2 蓝图调试

在 `HandleGizmoDragged` 中添加调试打印：
```
├─ Print String("Gizmo Dragged: " + DraggedActor.Name)
├─ Print String("Delta: " + DragDelta.ToString())
└─ Print String("Transform: " + GizmoTransform.ToString())
```

---

## 十二、相关文档

- `预览系统-Gizmo管理-系统设计.md` - Gizmo 系统设计
- `编辑系统-Controller-架构设计.md` - Controller 整体架构
- `VoxelControllerInteractComponent.h` - C++ 接口定义
- `VoxelControllerVisualUIComponent.h` - VisualUI 接口定义
- `Voxel3DUIActor.h` - 3D UI Actor 定义

---

**文档状态**：✅ 完整指南
