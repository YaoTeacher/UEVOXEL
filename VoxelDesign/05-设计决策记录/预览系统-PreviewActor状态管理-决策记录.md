# 预览系统 PreviewActor状态管理 决策记录

> **决策日期**：2026-01-16
> **决策状态**：✅ 已确定
> **相关文档**：`预览系统-PreviewActor-架构设计.md`、`预览系统-状态管理-系统设计.md`

---

## 一、问题背景

PreviewActor需要管理自身的渲染状态（流程状态、预览模式）。关键问题是：这些状态应该存储在哪里？

**核心问题**：PreviewActor的渲染状态（流程状态、预览模式）应该存储在PreviewActor中，还是其他地方？

**问题重要性**：
- 关系到职责划分
- 影响Actor自治性
- 关系到状态管理的一致性

---

## 二、现状分析

### 2.1 需要管理的状态

| 状态 | 类型 | 用途 |
|------|------|------|
| **PreviewState** | EPreviewInteractionState | 流程状态（HIDDEN/BASE/EDIT） |
| **CurrentMode** | EPreviewMode | 预览模式（None/SingleVoxel/SimpleGeometry/ComplexBlueprint） |

### 2.2 状态使用场景

```
流程状态（PreviewState）:
├─ 控制组件可见性
├─ 控制Gizmo显示
└─ 决定渲染行为

预览模式（CurrentMode）:
├─ 决定显示哪些组件
├─ 决定Gizmo配置
└─ 映射自编辑类型
```

---

## 三、候选方案

### 方案A：状态存储在VisualUIComponent

**描述**：PreviewActor不存储状态，所有状态由VisualUIComponent管理

**数据流**：
```
VisualUIComponent存储状态
└─ 渲染时传递状态给PreviewActor
```

**优点**：
- ✅ PreviewActor完全被动，职责单一（只负责渲染）
- ✅ 状态集中管理

**缺点**：
- ❌ PreviewActor失去自治性
- ❌ 每次渲染都需要传递状态
- ❌ PreviewActor无法自主响应状态变化
- ❌ 违反Actor自治原则

---

### 方案B：状态存储在StateComponent

**描述**：所有状态（包括渲染状态）存储在StateComponent中

**优点**：
- ✅ 状态集中管理

**缺点**：
- ❌ 职责混乱，StateComponent包含渲染状态
- ❌ 违反职责分离原则
- ❌ PreviewActor依赖性增强
- ❌ 业务数据和渲染状态混在一起

---

### 方案C：PreviewActor存储渲染状态，提供接口 ✅

**描述**：PreviewActor存储自身的渲染状态，提供接口供外部控制

**设计**：
```cpp
class AVoxelPreviewActor
{
private:
    EPreviewInteractionState PreviewState;
    EPreviewMode CurrentMode;

public:
    void SetPreviewState(EPreviewInteractionState NewState);
    void SetPreviewMode(EPreviewMode NewMode);
    EPreviewInteractionState GetPreviewState() const;
    EPreviewMode GetPreviewMode() const;

private:
    void OnPreviewStateChanged(EPreviewInteractionState OldState, EPreviewInteractionState NewState);
};
```

**优点**：
- ✅ Actor自治，存储自己的状态
- ✅ 可以自主响应状态变化
- ✅ 提供接口供外部控制
- ✅ 职责清晰（渲染状态在渲染层）
- ✅ 符合面向对象原则

**缺点**：
- ⚠️ 状态分散在不同组件

**状态同步**：
```
编辑类型（业务属性）:
└─ 存储在 StateComponent
   └─ VisualUIComponent 映射
      └─ 设置 PreviewActor 预览模式

流程状态（渲染状态）:
└─ 存储在 PreviewActor
   └─ VisualUIComponent 调用 SetPreviewState()
```

---

## 四、选择方案

### 选择方案C：PreviewActor存储渲染状态，提供接口

**决策理由**：

1. **Actor自治**：
   - PreviewActor是独立Actor，应该管理自己的状态
   - 符合面向对象的封装原则
   - PreviewActor可以自主响应状态变化

2. **职责清晰**：
   - 渲染状态在渲染层（PreviewActor）
   - 业务状态在业务层（StateComponent）
   - 职责分离，架构清晰

3. **性能考虑**：
   - 状态本地存储，访问快速
   - 无需跨组件查询
   - 响应及时

4. **控制接口**：
   - 提供Set/Get接口
   - VisualUIComponent调用接口控制状态
   - PreviewActor内部处理状态变化

5. **单一数据源**：
   - 每个状态只有一个存储位置
   - PreviewState存储在PreviewActor
   - 避免状态同步问题

---

## 五、状态管理机制

### 5.1 状态转换处理

```cpp
void AVoxelPreviewActor::SetPreviewState(EPreviewInteractionState NewState)
{
    if (PreviewState == NewState)
    {
        return;  // 状态未变化，直接返回
    }

    EPreviewInteractionState OldState = PreviewState;
    PreviewState = NewState;

    // 内部处理状态变更
    OnPreviewStateChanged(OldState, NewState);
}
```

**设计特点**：
- 检查状态是否变化
- 触发内部处理逻辑
- PreviewActor自主响应

### 5.2 状态处理方法

| 方法 | 触发状态 | 职责 |
|------|---------|------|
| **HandleHiddenState** | HIDDEN | 隐藏所有组件和Gizmo，清除包围盒数据 |
| **HandleBaseState** | BASE | 显示2D包围盒相关组件，隐藏Gizmo |
| **HandleEditState** | EDIT | 显示3D包围盒和所有Gizmo，更新Gizmo位置 |

### 5.3 状态同步规则

```
编辑类型变更 → VisualUIComponent处理
├─ 清空编辑数据
├─ 更新 StateComponent 编辑类型
└─ 调用 PreviewActor.SetPreviewState(HIDDEN)

流程状态变更 → VisualUIComponent发起
└─ 调用 PreviewActor.SetPreviewState(NewState)
   └─ PreviewActor内部处理
      └─ 更新组件可见性
      └─ 更新Gizmo显示

预览模式变更 → VisualUIComponent发起
└─ 调用 PreviewActor.SetPreviewMode(NewMode)
   └─ PreviewActor内部处理
      └─ 更新组件可见性
      └─ 更新Gizmo配置
```

---

## 六、影响分析

### 6.1 正面影响

| 影响项 | 说明 |
|--------|------|
| **Actor自治** | ✅ PreviewActor管理自己的状态 |
| **职责清晰** | ✅ 渲染状态在渲染层，业务状态在业务层 |
| **响应及时** | ✅ 状态变化时立即响应 |
| **架构清晰** | ✅ 分层明确，易于理解 |

### 6.2 负面影响

| 影响项 | 说明 | 缓解措施 |
|--------|------|---------|
| 状态分散 | 状态分布在不同组件 | 文档清晰，职责明确 |
| 需要同步 | 预览模式需要从编辑类型映射 | VisualUIComponent统一映射 |

### 6.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 状态不一致 | 低 | 中 | VisualUIComponent统一管理，单一数据源 |
| 状态同步遗漏 | 低 | 中 | Code Review，测试覆盖 |

---

## 七、实施计划

### 7.1 实施步骤

1. **定义状态成员**
   ```cpp
   EPreviewInteractionState PreviewState;
   EPreviewMode CurrentMode;
   ```

2. **实现状态控制接口**
   ```cpp
   void SetPreviewState(EPreviewInteractionState NewState);
   void SetPreviewMode(EPreviewMode NewMode);
   EPreviewInteractionState GetPreviewState() const;
   EPreviewMode GetPreviewMode() const;
   ```

3. **实现状态处理方法**
   ```cpp
   void OnPreviewStateChanged(EPreviewInteractionState OldState, EPreviewInteractionState NewState);
   void HandleHiddenState();
   void HandleBaseState();
   void HandleEditState();
   ```

4. **集成到VisualUIComponent**
   - VisualUIComponent映射编辑类型到预览模式
   - VisualUIComponent调用PreviewActor接口设置状态

### 7.2 验证方法

| 验证项 | 方法 |
|--------|------|
| 状态转换正确 | 单元测试 |
| 组件可见性正确 | 集成测试 |
| 状态同步正确 | 手动测试 |

---

## 八、相关决策

| 相关决策 | 文档 |
|---------|------|
| PreviewActor数据访问 | `预览系统-PreviewActor数据访问-决策记录.md` |
| Gizmo分组管理 | `预览系统-Gizmo分组管理-决策记录.md` |

---

## 九、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状态**：✅ 已确定
