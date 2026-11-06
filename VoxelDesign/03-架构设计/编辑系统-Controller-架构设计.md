# 编辑系统 Controller 架构设计

> **文档目的**：定义 VoxelPlayerController 的目标架构、组件关系和接口设计
> **创建日期**：2026-01-16
> **版本**：v2.0（2026-07-17 重写）
> **状态**：已确定（目标架构）
>
> **v2.0 更新**：状态组件按域拆分、细粒度事件、统一鼠标射线、输入路由保留蓝图、删除 EditorModeSubsystem、Undo/Redo 业务归位 HistoryComponent。旧版 v1.x 见 `_archive/编辑系统-Controller-架构设计-20260717.md`

---

## 一、概述

### 1.1 Controller 定位

**VoxelPlayerController** 是玩家控制器，是玩家交互的入口点。

**核心定位**：
- 持有和管理按域划分的 Component
- 提供 BlueprintCallable 入口，接收蓝图转发的输入事件
- 不直接处理业务逻辑和渲染

**单机前提（显式声明）**：
- 本项目为单机游戏定位，组件挂 PlayerController、状态不做网络复制
- 若未来联机，选区/手持物等状态需迁移至 PlayerState 并添加复制

### 1.2 设计原则

1. **按域拆分**：状态按业务域拆分为独立组件，禁止 God Object
2. **细粒度事件**：每个域只广播自己的变更，订阅方按需重建，禁止"一个广播打所有"
3. **单向数据流**：输入 → 工具 → 状态组件 →（细粒度委托）→ 预览/UI
4. **输入路由在蓝图**：蓝图绑定 IA 事件调用 C++ 入口；C++ 不做 IA 绑定，但保留 UPROPERTY 资产引用兜底
5. **轮询归 C++**：Hover 等非事件型检测由 C++ 低频定时器驱动，蓝图不跑每帧 Tick

---

## 二、整体架构

### 2.1 组件结构

```
AVoxelPlayerController（薄协调层）
├─ 输入入口（BlueprintCallable）
│    蓝图：IA 事件绑定 → 调用 C++ 入口
│    C++：UPROPERTY(EditDefaultsOnly) 持有 12 个 IA + IMC 资产引用（兜底/审计）
│
├─ UVoxelToolComponent         工具注册表 + 输入分发 + 统一鼠标射线
├─ UVoxelBuildStateComponent   建造会话状态（建造域）
├─ UVoxelSelectionComponent    选区与剪切板（选区域）
├─ UVoxelPreviewComponent      预览与 Gizmo 管理（渲染层，只订阅不写入）
└─ UVoxelHistoryComponent      Undo/Redo 业务 + 栈（栈操作私有化）
```

### 2.2 状态域划分

原 `UVoxelControllerStateComponent`（715 行 God Object）按域拆分：

| 组件 | 持有状态 | 细粒度事件 |
|---|---|---|
| `UVoxelBuildStateComponent` | 控制器模式、当前工具 Tag、TmpGeometry、笔刷、切面、手持物、BuildPhase | `OnControllerModeChanged` / `OnToolChanged` / `OnGeometryChanged` / `OnBrushChanged` / `OnSliceChanged` / `OnItemInHandChanged` |
| `UVoxelSelectionComponent` | 选区、PreDragSelection、剪切板 | `OnSelectionChanged` / `OnClipboardChanged` |

**TmpGeometry 归属说明**：TmpGeometry 是建造会话的工作数据，完整链路（工具写入 → 存储 → 预览重建）都在建造域内，归 BuildStateComponent。多工具"公用"不构成问题——工具经 BuildStateComponent 读写，与拆分前经 StateComponent 读写等价。拆分解决的不是共享，而是事件粒度。

### 2.3 统一鼠标射线

**现状问题**：同一条 Channel1 地面射线每帧被 3 处各跑一遍（蓝图 Tick→HandleMouseHover、VisualUIComp::UpdateGroundMarker、Brush 拖拽）。

**目标设计**：

```
ToolComponent 内低频定时器（50ms）驱动：
  DeprojectMousePositionToWorld → 世界射线 (Start/End)  ← 唯一来源
    ├─ 地面追踪（ECC_GameTraceChannel1）每周期一次 → 共享结果 FVoxelMouseHit
    │     分发给：当前工具 OnMouseHover、PreviewComponent GroundMarker、Brush 涂抹
    └─ 按需追踪（复用同一 Start/End，仅当前激活工具需要时发起）：
          家具工具 → ECC_GameTraceChannel2（家具检测）
          区域工具 → ECC_GameTraceChannel3（区域检测）
          EDIT 阶段 → ECC_GameTraceChannel6（Gizmo 检测）
```

**规则**：世界射线只有一条；地面追踪每周期只跑一次；业务通道追踪由当前激活工具按需发起。蓝图 Tick 的 Hover 调用与 VisualUIComp 的重复地面射线全部删除。

---

## 三、核心组件设计

### 3.1 UVoxelToolComponent

**职责**：
- 工具注册表（8 个工具实例，按 GameplayTag 注册，NewObject Outer=本组件，UPROPERTY 持有）
- 输入分发：`HandleMouseInput / HandleMouseRelease / HandleMouseCancel / HandleKeyDown` 组装 `FVoxelToolContext` 转发当前工具
- 统一鼠标射线：持有低频定时器，产出共享 `FVoxelMouseHit`，驱动 Hover/GroundMarker/Brush
- 家具长按：`FTimerHandle` 判定长按（替代 Tick 轮询 `GetTimeSeconds` 比对）

**不做**：不持有业务状态、不渲染、不做 UI

### 3.2 UVoxelBuildStateComponent

**职责**：
- 存储建造域全部状态（模式/工具/几何/笔刷/切面/手持物/阶段）
- Setter 内做变化检测，**变化才广播**对应的细粒度事件（修复 `SetCurrentToolTag` 未变化也强制广播的问题）
- 动态组装 `FVoxelEditRequest`

**不做**：不处理输入、不渲染、不访问世界子系统

### 3.3 UVoxelSelectionComponent

**职责**：选区/预拖选区/剪切板的存储与细粒度广播

### 3.4 UVoxelPreviewComponent

**职责**：
- 持有 PreviewActor/SelectionPreviewActor/Gizmo Actor（Spawn 时 `Owner = PlayerController`）
- 订阅 BuildState/Selection 的细粒度委托，**只重建受影响的预览**
- `EndPlay` 统一销毁所持 Actor（修复滞留问题）
- 只读状态、不回写（删除 `SetPreDragSelection` 回写路径）

**不做**：不处理业务逻辑、不做射线（射线结果由 ToolComponent 传入）

### 3.5 UVoxelHistoryComponent

**职责**：
- Undo/Redo 完整业务：弹栈 → 反向操作 → 调子系统回滚 → 失败回退 → 压栈
- 栈操作私有化（修复裸 public 栈接口由调用方手工维持不变量的问题）
- 操作日志与统计

原 InteractComp 中 200 行手工栈管理（`VoxelControllerInteractComponent.cpp:1307-1506`）迁入本组件。

### 3.6 职责迁移总表

| 原组件 | 去向 |
|---|---|
| StateComponent（建造域状态） | BuildStateComponent |
| StateComponent（选区/剪切板） | SelectionComponent |
| InteractComp（工具注册/输入分发/射线/长按） | ToolComponent |
| InteractComp（Undo/Redo 业务） | HistoryComponent |
| InteractComp（Gizmo 数学） | ToolComponent（交互数学） |
| VisualUIComp（全部） | PreviewComponent |
| EditorModeSubsystem | **删除**（模式权威为 BuildStateComponent::PlayerMode） |

---

## 四、输入设计

### 4.1 职责划分

| 层 | 职责 |
|---|---|
| 蓝图（BP_PlayerController） | 绑定 IA 事件节点 → 调用 C++ 入口；策划可改键 |
| C++（PC/ToolComponent） | 提供 BlueprintCallable 入口；持有 IA/IMC 资产 UPROPERTY 引用兜底；Hover 等轮询由内部定时器驱动 |

### 4.2 输入路由表（蓝图绑定 → C++ 入口）

| IA | C++ 入口 |
|---|---|
| IA_Select | ToolComponent::HandleMouseInput |
| IA_Release | ToolComponent::HandleMouseRelease |
| IA_Escape | UI 层返回栈（迁移后由 CommonUI 托管） |
| IA_Edit_Undo / IA_Edit_Redo | HistoryComponent::Undo / Redo |
| IA_Edit_Delete | ToolComponent::DeleteSelectedFurniture |
| IA_Move / IA_Look / 相机系列 | PC 转发 ControlledPawn |

### 4.3 清理项

- 删除 TopDown 模板残留：5 个空输入 handler、`SetDestinationClickAction/TouchAction`、`FXCursor`、`CachedDestination` 等
- 删除每帧 Error 级相机日志（`VoxelPlayerController.cpp:174`）与 Gizmo 拖拽 ~140 行 Warning 调试日志
- Ctrl 修饰键：蓝图侧保留（策划可调），C++ 路由表注释中登记

---

## 五、组件交互与数据流

### 5.1 依赖关系

```
ToolComponent
  ├─ 读写：BuildStateComponent / SelectionComponent
  ├─ 调用：HistoryComponent（记录）、世界子系统（执行编辑）
  └─ 产出：FVoxelMouseHit（共享射线结果）

PreviewComponent
  ├─ 订阅：BuildStateComponent / SelectionComponent 细粒度委托（只读）
  ├─ 接收：ToolComponent 的 FVoxelMouseHit（GroundMarker）
  └─ 持有：PreviewActor / Gizmo Actor（Owner=PC，EndPlay 清理）

HistoryComponent
  └─ 调用：世界子系统（Undo/Redo 回滚）
```

### 5.2 完整数据流

```
蓝图 IA 事件 → ToolComponent 分发 → 当前 Tool 处理
  → 写 BuildStateComponent / SelectionComponent（变化才广播细粒度事件）
       ├→ PreviewComponent 只重建受影响的预览
       ├→ HistoryComponent 记录
       └→ ViewModel(FieldNotify) → Widget 自动刷新（见 UI系统-架构设计）
```

### 5.3 跨系统通知

世界子系统 → UI 的跨系统事件（Toast、模式切换）走 GameplayMessageSubsystem 消息频道，不走组件直连委托。

---

## 六、生命周期管理

### 6.1 组件初始化顺序

PC 构造函数 `CreateDefaultSubobject` 创建全部组件（顺序确定）；`BeginPlay` 中由 PC 统一完成组件间引用注入（ToolComponent 注入 BuildState/Selection/History；PreviewComponent 注入 BuildState/Selection），消除组件各自 BeginPlay 顺序不确定导致的绑定竞态（废除 `RequestControllerModeSync` 补丁）。

### 6.2 组件销毁

- 组件随 PC 销毁
- PreviewComponent `EndPlay` 先销毁所持场景 Actor（PreviewActor/Gizmo），防止 PC 先于 World 销毁时 Actor 滞留

---

## 七、关键修复清单（随 v2.0 架构落地）

| 问题 | 位置 | 修复 |
|---|---|---|
| GameMode::GameState 从未赋值 | `VoxelGameMode.cpp:64-67` | 赋值/改用 `AGameModeBase::GameState` |
| PC BeginPlay 对 GameMode 无空检查 | `VoxelPlayerController.cpp:51-54` | 加空检查 |
| `IsBlockIndexInScene` 永真 | `VoxelBlockWorldSubsystem.h:270` | 实现真实边界判断 |
| `CastChecked<UEditorEngine>` 无守卫 | `InteractionWidget.cpp:38-40` | `#if WITH_EDITOR` |
| `SetActorLabel` 7 处无守卫 | VisualUIComp/Creature/BlockWorld/ChunkBase | `#if WITH_EDITOR` |
| GameplayTag 缺陷 ×3 | `CoreTags.cpp:125` 双点、`Creature/Item` 双轨、`Effect.*` 未定义 | 修正/去重/补定义 |
| 删除家具硬编码 /100.0f | `InteractComp.cpp:2102` | 走 WorldConfig.BlockLength |
| `GetFirstPlayerController()` ×7 | 各 UI 文件 | `GetOwningPlayer()` |

---

## 八、类关系图

```
APlayerController
  └─ AVoxelPlayerController
       ├─ UVoxelToolComponent ──> UVoxelToolBase × 8（TMap<FGameplayTag, ...>）
       ├─ UVoxelBuildStateComponent
       ├─ UVoxelSelectionComponent
       ├─ UVoxelPreviewComponent ──> AVoxelPreviewActor / AVoxelGizmoActor（Owner=PC）
       └─ UVoxelHistoryComponent（UndoStack/RedoStack 私有）

APawn
  └─ AVoxelControllerPawn（相机 Pawn，逻辑不变）
```

---

## 九、附录

### 9.1 相关文档

- [UI系统-架构设计](/VoxelDesign/03-架构设计/UI系统-架构设计.md)
- [预览系统-PreviewActor-架构设计](/VoxelDesign/03-架构设计/预览系统-PreviewActor-架构设计.md)
- [前端架构-UE5规范化-决策记录](/VoxelDesign/05-设计决策记录/前端架构-UE5规范化-决策记录.md)

### 9.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本（已归档） |
| v1.1/v1.2 | 2026-01-19 | 按实际代码更新、补充蓝图层说明（已归档） |
| v2.0 | 2026-07-17 | 重写：状态按域拆分、细粒度事件、统一鼠标射线、输入保留蓝图+轮询归 C++、Undo/Redo 归位、删除 EditorModeSubsystem、显式单机前提 |
