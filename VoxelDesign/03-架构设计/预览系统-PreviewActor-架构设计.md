# PreviewActor 架构设计

> **文档目的**：定义 PreviewActor 及预览管理的目标架构
> **系统定位**：PreviewActor 是预览系统的核心渲染组件，服务于编辑系统
> **创建日期**：2026-01-16
> **版本**：v3.0（2026-07-17 重写）
> **状态**：已确定（目标架构）
>
> **v3.0 更新**：明确所有权与生命周期、按域增量更新、Gizmo 按需创建、禁止回写业务状态。旧版 v2.x 见 `_archive/预览系统-PreviewActor-架构设计-20260717.md`

---

## 一、概述

### 1.1 定位

**PreviewActor** 是 UVoxelPreviewComponent 管理的独立 Actor，专注建造预览的**纯渲染**。

**核心定位**（保持 v2.0 原则不变）：
- 独立 AActor，被 PreviewComponent 持有
- 只渲染，不处理业务逻辑，不查询状态组件，不回写业务状态
- 单向数据流：状态组件 → PreviewComponent → PreviewActor

### 1.2 设计原则

1. **所有权明确**：Spawn 时 `Owner = PlayerController`，PreviewComponent `EndPlay` 统一销毁
2. **按需创建**：Gizmo 不再常驻 12 个，进入 EDIT 阶段按需创建、退出即销毁
3. **增量更新**：按域细分更新入口，任何状态变更只重建受影响的渲染，禁止全量 `GenerateMeshData`
4. **只读原则**：PreviewActor 不查询状态组件、不写入任何业务数据

---

## 二、管理结构

```
UVoxelPreviewComponent（PC 组件，管理层）
   ├─ AVoxelPreviewActor          建造预览（1 个常驻，Owner=PC）
   ├─ AVoxelSelectionPreviewActor 选区高亮（1 个常驻，Owner=PC）
   └─ AVoxelGizmoActor × N        Gizmo（按需创建/销毁）
```

| 角色 | 生命周期 |
|---|---|
| PreviewActor / SelectionPreviewActor | PreviewComponent BeginPlay 创建（Owner=PC），EndPlay 销毁 |
| Gizmo Actor | 进入 EDIT 阶段按 Gizmo 类型创建，退出 EDIT/切换工具时销毁 |

---

## 三、接口设计

### 3.1 PreviewActor 更新入口（按域细分）

| 接口 | 触发源（细粒度事件） | 行为 |
|---|---|---|
| `UpdateFromEditRequest(Request)` | 复合变更（保留的统一入口） | 全量刷新（仅初始化/工具切换时） |
| `UpdateGeometry(Geometry)` | `OnGeometryChanged` | 只重建几何预览（线框/厚度/模板网格） |
| `UpdateSelection(Box)` | `OnSelectionChanged` | 只更新选区高亮 |
| `SetPreviewMode(Mode)` | `OnToolChanged` | 只切显隐与模式，不重建网格 |
| `SetPreviewVisible(bVisible)` / `ClearPreview()` | PreviewComponent | 显隐控制 |

**性能规则**：拖拽 Gizmo 改参数等高频操作只走 `UpdateGeometry`；切工具只走 `SetPreviewMode`；禁止所有变更都全量重建（修复旧 `HandleBuildModeParamsUpdated` 240 行分支 + 每次 `GenerateMeshData` 全量重建的问题）。

### 3.2 PreviewComponent 事件订阅

```
BuildStateComponent.OnToolChanged      → PreviewActor.SetPreviewMode
BuildStateComponent.OnGeometryChanged  → PreviewActor.UpdateGeometry
BuildStateComponent.OnBrushChanged     → 笔刷预览增量更新
BuildStateComponent.OnSliceChanged     → 切面参数应用（见 3.3）
SelectionComponent.OnSelectionChanged  → SelectionPreviewActor.UpdateSelection
ToolComponent 共享射线结果              → GroundMarker 位置更新（不自做地面射线）
```

### 3.3 切面应用

**架构（v3.1 重构）**：切面效果由 **CPU 三域面分流 + 双 Section 网格** 实现，详见 [切面系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-切面系统-架构设计.md)。

**核心流程**：
- `BuildStateComponent.OnSliceChanged` -> `PreviewComponent.ApplySliceVisuals()`
  - ① `BlockWorldSubsystem::ApplySlice`：缓存 Slice 快照（SliceLayer + 区域盒）-> dirty chunk 标记 `WToCalculate` -> 复用异步队列重建（`BuildChunkContext` 注入快照 -> `GenerateMeshData` 三域双输出 -> 双 Section ApplyMeshRender）
  - ② `ApplySliceToFurnitureAndCreatures`：跳过 `AGreedyChunk`（修复 SharedChunkMID 污染），家具/生物 MID 走 `MF_SliceDither` 材质函数
- 切面区域动态更新：`CheckSliceAreaUpdate` 玩家移动 320cm + 0.5s 双节流

**已修复问题**：
- ~~`ApplySliceToFurnitureAndCreatures` 全场景遍历含 chunk -> 覆写 SharedChunkMID.IsCheckZTrans（last-writer-wins 致全局开关失效）~~ -> 跳过 AGreedyChunk
- ~~材质 Z 判定 Constant_3=0 致超高体素消失而非半透明、A==B 悬空致切面层顶面缺失~~ -> CPU 三域分流，材质去参数化
- ~~区域边界 chunk 整切致区域外侧面墙壁失碰撞~~ -> 逐体素精确判定，Opaque 域保留碰撞

### 3.4 禁止项

| 禁止 | 说明 |
|---|---|
| 回写业务状态 | 删除渲染回调中 `SetPreDragSelection` 等回写路径（重入广播风险） |
| 自做地面射线 | GroundMarker 位置来自 ToolComponent 共享射线结果（删除 `UpdateGroundMarker` 内重复 LineTrace） |
| 无 Owner Spawn | 所有 SpawnActor 必须 `Owner = PC`，EndPlay 清理 |
| 常驻 Gizmo | 12 个 Gizmo 常驻改按需创建 |

---

## 四、数据接收原则（沿用 v2.1）

```
✅ 允许：从 FVoxelEditRequest / 增量接口参数读取渲染所需全部数据
❌ 禁止：主动查询状态组件、修改传入数据、写入任何业务数据
```

---

## 五、生命周期管理

### 5.1 创建

```cpp
// PreviewComponent::BeginPlay
FActorSpawnParameters Params;
Params.Owner = GetOwner(); // PlayerController
PreviewActor = GetWorld()->SpawnActor<AVoxelPreviewActor>(..., Params);
```

### 5.2 销毁

```cpp
// PreviewComponent::EndPlay
// 统一 Destroy：PreviewActor / SelectionPreviewActor / 全部存活 Gizmo
// 防止 PC 先于 World 销毁时 Actor 滞留场景
```

### 5.3 Gizmo 生命周期

```
进入 EDIT 阶段（OnGeometryChanged 且阶段=EDIT）
  → 按需 SpawnGizmo(Type)（Owner=PC）
退出 EDIT / 切换工具
  → DestroyGizmos()
```

---

## 六、与编辑系统的关系

### 6.1 数据流

```
BuildStateComponent / SelectionComponent
  ↓ 细粒度委托（OnGeometryChanged / OnSelectionChanged / OnToolChanged）
UVoxelPreviewComponent（只读订阅）
  ↓ 按域增量接口
AVoxelPreviewActor（纯渲染执行）
```

### 6.2 接口边界

- **编辑系统 → PreviewActor**：`FVoxelEditRequest` / 增量参数（Geometry/Box/Mode）
- **PreviewActor → 编辑系统**：无（单向，不回调）

---

## 七、附录

### 7.1 相关文档

- [编辑系统-Controller-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller-架构设计.md)
- [UI系统-架构设计](/VoxelDesign/03-架构设计/UI系统-架构设计.md)
- [编辑系统-预览系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-预览系统-架构设计.md)

### 7.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.x | 2026-01-16 | 初始版本（已归档） |
| v2.0/v2.1 | 2026-01-18 | 移除 Gizmo 管理、明确服务编辑系统定位（已归档） |
| v3.0 | 2026-07-17 | 所有权与 EndPlay 生命周期、按域增量更新入口、Gizmo 按需创建、切面应用优化、禁回写/禁重复射线 |
