# 预览系统 PreviewActor统一更新接口 决策记录

> **决策日期**：2026-01-17
> **决策状态**：✅ 已确定
> **相关文档**：`预览系统-PreviewActor-架构设计.md`

---

## 一、问题背景

PreviewActor需要接收Geometry数据变化并更新所有子项（渲染+Gizmo）。关键问题是：应该设计什么样的接口来实现统一更新？

**核心问题**：
1. PreviewActor应该如何接收Geometry变化？
2. 是通过VisualUIComponent转发，还是直接绑定StateComponent广播？
3. 接口应该传递哪些参数？

**问题重要性**：
- 关系到架构的清晰性和一致性
- 影响PreviewActor的职责划分
- 关系到数据流的单向性

---

## 二、现状分析

### 2.1 当前实现

**VisualUIComponent已绑定StateComponent广播**：
```cpp
// VoxelControllerVisualUIComponent.cpp:408
SC->OnTmpGeometryChanged.AddDynamic(this, &UVoxelControllerVisualUIComponent::HandleTmpGeometryChanged);
```

**VisualUIComponent主动调用PreviewActor的多个更新方法**：
```cpp
void HandleTmpGeometryChanged(const FVoxelGeometryDesc& NewGeometry)
{
    // 需要判断调用哪个方法
    if (bIsSingleVoxel)
        PreviewActor->UpdateSingleVoxelPreview(...);
    else
        PreviewActor->UpdateSimpleGeometryPreview(...);

    // Gizmo更新需要PreviewActor主动查询StateComponent
    PreviewActor->UpdateGizmosPosition();  // 内部查询StateComponent
}
```

### 2.2 存在的问题

| 问题 | 影响 |
|------|------|
| **接口分散** | VisualUIComponent需要知道何时调用哪个方法 |
| **职责混乱** | Gizmo更新绕过VisualUIComponent，PreviewActor主动查询StateComponent |
| **架构不一致** | 渲染更新通过广播，Gizmo更新主动查询 |
| **参数冗余** | 如果设计统一接口，是否需要传递BuildState/EditState/BlockType？ |

---

## 三、候选方案

### 方案A：统一接口 + 只传Geometry ✅

**描述**：PreviewActor提供统一更新接口，只接收Geometry参数，其他信息按需查询

**数据流**：
```
StateComponent → OnTmpGeometryChanged → VisualUIComponent
  └→ PreviewActor.UpdateFromGeometry(Geometry)
       ├→ 按需查询BuildState/EditState/BlockType
       ├→ 判断渲染模式
       ├→ 调用UpdateXxxPreview()
       └→ 更新Gizmo
```

**接口设计**：
```cpp
// 统一入口
void UpdateFromGeometry(const FVoxelGeometryDesc& Geometry);

// 内部辅助方法
struct FRenderContext
{
    EVoxelControllerBuildState BuildState;
    EVoxelControllerEditState EditState;
    FGameplayTag BlockType;
};

FRenderContext GetRenderContext() const;  // 按需查询
void UpdateRenderFromGeometry(const FVoxelGeometryDesc& Geometry, const FRenderContext& Context);
void UpdateGizmosFromGeometry(const FVoxelGeometryDesc& Geometry);
```

**优点**：
- ✅ 接口简洁：只传一个Geometry参数
- ✅ 职责清晰：Geometry = 几何描述，不混杂业务状态
- ✅ 自包含：PreviewActor按需查询业务状态
- ✅ 单向数据流：VisualUIComponent → PreviewActor
- ✅ 易于测试：传入任意Geometry即可测试
- ✅ 符合架构文档：VisualUIComponent统一管理PreviewActor

**缺点**：
- ⚠️ PreviewActor需要访问StateComponent（已有机制：OwningController）

---

### 方案B：统一接口 + 传递所有参数

**描述**：接口传递Geometry + BuildState + EditState + BlockType

**接口设计**：
```cpp
void UpdateFromGeometry(
    const FVoxelGeometryDesc& Geometry,
    EVoxelControllerBuildState BuildState,
    EVoxelControllerEditState EditState,
    FGameplayTag BlockType
);
```

**优点**：
- ✅ PreviewActor无需查询StateComponent
- ✅ 所有数据显式传递

**缺点**：
- ❌ 接口冗余：BuildState/EditState/BlockType不是几何信息
- ❌ 职责混乱：几何描述混杂业务状态
- ❌ 扩展困难：未来新增业务状态需要改接口
- ❌ 违反单一职责原则：Geometry应该只描述几何

---

### 方案C：PreviewActor直接绑定StateComponent广播

**描述**：PreviewActor绕过VisualUIComponent，直接绑定OnTmpGeometryChanged

**数据流**：
```
StateComponent → OnTmpGeometryChanged
  ├→ VisualUIComponent（更新渲染）
  └→ PreviewActor（更新Gizmo）
```

**优点**：
- ✅ Gizmo更新和渲染更新解耦

**缺点**：
- ❌ 绕过VisualUIComponent：违反"VisualUIComponent管理PreviewActor"的设计
- ❌ 架构不一致：为什么渲染通过VisualUIComponent，Gizmo不用？
- ❌ 双重绑定：StateComponent广播被两个组件同时绑定
- ❌ 违反架构文档：文档明确VisualUIComponent是唯一管理者

---

### 方案D：保持现状（混合模式）

**描述**：VisualUIComponent调用渲染更新，PreviewActor主动查询Gizmo更新

**优点**：
- ✅ 无需改动

**缺点**：
- ❌ 架构不统一：广播 + 主动查询混合
- ❌ 绕过管理者：PreviewActor绕过VisualUIComponent
- ❌ 职责模糊：PreviewActor既是被动接收者，又是主动查询者

---

## 四、选择方案

### 选择方案A：统一接口 + 只传Geometry

**决策理由**：

1. **接口简洁性** ⭐⭐⭐⭐⭐
   - 只传一个参数：Geometry
   - Geometry本身就是完整的几何描述
   - 符合"接口应该最小化"原则

2. **职责清晰性** ⭐⭐⭐⭐⭐
   - Geometry = 几何描述
   - 业务状态（BuildState/EditState/BlockType）不是几何信息
   - PreviewActor按需查询，保持几何描述的纯净性

3. **架构一致性** ⭐⭐⭐⭐⭐
   - 符合"VisualUIComponent统一管理PreviewActor"
   - 单向数据流：VisualUIComponent → PreviewActor
   - 所有更新都通过VisualUIComponent转发

4. **符合设计文档** ⭐⭐⭐⭐⭐
   - 架构文档明确：PreviewActor"接收VisualUIComponent的控制指令"
   - 数据访问决策记录：PreviewActor通过OwningController访问StateComponent

5. **易于测试和维护** ⭐⭐⭐⭐
   - 单元测试：传入任意Geometry即可
   - 集成测试：只需测试一个入口
   - 扩展性：新增字段无需改接口

---

## 五、设计细节

### 5.1 接口定义

**公共接口**：
```cpp
/**
 * 从Geometry统一更新所有预览子项（渲染+Gizmo）
 *
 * @param Geometry 几何描述数据（包含所有几何信息）
 *
 * @note PreviewActor会根据Geometry和内部状态（PreviewState/CurrentMode）
 *       自动决定如何渲染和显示Gizmo
 */
UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
void UpdateFromGeometry(const FVoxelGeometryDesc& Geometry);
```

**内部辅助结构**：
```cpp
// 渲染上下文（按需查询的业务状态）
struct FRenderContext
{
    EVoxelControllerBuildState BuildState;
    EVoxelControllerEditState EditState;
    FGameplayTag BlockType;
};
```

**内部方法**：
```cpp
// 获取渲染上下文（从Controller查询业务状态）
FRenderContext GetRenderContext() const;

// 根据Geometry和Context更新渲染
void UpdateRenderFromGeometry(
    const FVoxelGeometryDesc& Geometry,
    const FRenderContext& Context
);

// 根据Geometry更新Gizmo
void UpdateGizmosFromGeometry(const FVoxelGeometryDesc& Geometry);
```

### 5.2 实现逻辑

```cpp
void AVoxelPreviewActor::UpdateFromGeometry(const FVoxelGeometryDesc& Geometry)
{
    // 1. 验证Geometry有效性
    if (!Geometry.IsValid())
    {
        ClearPreview();
        HideAllGizmos();
        return;
    }

    // 2. 按需查询业务状态
    FRenderContext Context = GetRenderContext();

    // 3. 更新渲染
    UpdateRenderFromGeometry(Geometry, Context);

    // 4. 更新Gizmo
    UpdateGizmosFromGeometry(Geometry);
}
```

### 5.3 VisualUIComponent调用

**修改前**（分散调用）：
```cpp
void HandleTmpGeometryChanged(const FVoxelGeometryDesc& NewGeometry)
{
    // 需要判断逻辑
    if (bIsSingleVoxel)
        PreviewActor->UpdateSingleVoxelPreview(...);
    else
        PreviewActor->UpdateSimpleGeometryPreview(...);

    PreviewActor->UpdateGizmosPosition();  // 主动查询
}
```

**修改后**（统一调用）：
```cpp
void HandleTmpGeometryChanged(const FVoxelGeometryDesc& NewGeometry)
{
    if (PreviewActor)
    {
        PreviewActor->UpdateFromGeometry(NewGeometry);
    }
}
```

---

## 六、影响分析

### 6.1 正面影响

| 影响项 | 说明 |
|--------|------|
| **接口统一** | ✅ 只需调用一个方法，更新所有子项 |
| **职责清晰** | ✅ VisualUIComponent管理，PreviewActor执行 |
| **架构一致** | ✅ 单向数据流，无绕过管理者的情况 |
| **易于维护** | ✅ 逻辑集中在PreviewActor内部 |
| **易于测试** | ✅ 单一入口，参数简单 |

### 6.2 负面影响

| 影响项 | 说明 | 缓解措施 |
|--------|------|---------|
| 内部查询 | PreviewActor需要查询StateComponent | 已有机制：OwningController弱引用 |

### 6.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 性能问题 | 低 | 低 | 查询开销极小（指针访问） |
| StateComponent无效 | 低 | 中 | 弱引用+有效性检查 |
| 渲染逻辑错误 | 低 | 中 | 单元测试覆盖 |

---

## 七、实施计划

### 7.1 实施步骤

1. **更新架构设计文档**
   - 添加UpdateFromGeometry接口定义
   - 标注其他UpdateXxxPreview为内部实现
   - 更新数据流图

2. **创建决策记录**
   - 记录统一接口的设计决策
   - 记录方案对比和选择理由

3. **修改PreviewActor头文件**
   - 添加UpdateFromGeometry公共接口
   - 添加内部辅助结构和方法

4. **修改PreviewActor实现**
   - 实现UpdateFromGeometry
   - 实现GetRenderContext
   - 实现UpdateRenderFromGeometry
   - 实现UpdateGizmosFromGeometry（移除主动查询）
   - 保留现有UpdateXxxPreview作为内部实现

5. **修改VisualUIComponent**
   - 简化HandleTmpGeometryChanged，只调用UpdateFromGeometry

6. **测试验证**
   - 单元测试：验证渲染模式判断
   - 集成测试：验证完整数据流
   - 手动测试：验证视觉效果

### 7.2 验证方法

| 验证项 | 方法 |
|--------|------|
| 接口调用正确 | 单元测试 |
| 渲染更新正确 | 视觉测试 |
| Gizmo更新正确 | 交互测试 |
| 性能无退化 | 性能测试 |

---

## 八、相关决策

| 相关决策 | 文档 |
|---------|------|
| PreviewActor数据访问 | `预览系统-PreviewActor数据访问-决策记录.md` |
| PreviewActor状态管理 | `预览系统-PreviewActor状态管理-决策记录.md` |

---

## 九、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-17 | 初始版本 |

---

**决策状态**：✅ 已确定
