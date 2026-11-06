# 预览系统 Geometry操作回调机制 决策记录

> **决策日期**：2026-01-16
> **决策状态**：✅ 已确定
> **相关文档**：`预览系统-PreviewActor-架构设计.md`、`编辑系统-FVoxelGeometryDesc-系统设计.md`

---

## 一、问题背景

当用户拖动Gizmo时，PreviewActor需要通知外部执行Geometry操作。关键问题是：PreviewActor应该如何通知VisualUIComponent执行Geometry操作？

**核心问题**：Gizmo拖动时，PreviewActor如何通知外部执行Geometry操作？

**问题重要性**：
- 关系到职责分离
- 影响数据流清晰性
- 关系到单向数据流原则

---

## 二、需求分析

### 2.1 功能需求

1. **Gizmo交互检测**：PreviewActor检测Gizmo拖动
2. **操作通知**：通知VisualUIComponent执行对应操作
3. **参数传递**：传递操作所需的参数

### 2.2 约束条件

- PreviewActor是渲染层，不应该修改业务数据
- 业务逻辑由InteractComponent处理
- 必须保持单向数据流

### 2.3 质量属性

- **职责清晰**：渲染层不处理业务逻辑
- **数据流清晰**：操作流向明确
- **可维护性**：代码易于理解和维护

---

## 三、候选方案

### 方案A：PreviewActor直接调用InteractComponent

**描述**：PreviewActor存储InteractComponent指针，直接调用InteractComponent的方法

**数据流**：
```
用户拖动Gizmo
└─ PreviewActor检测拖动
   └─ 直接调用 InteractComponent->MoveGEO(Delta)
      └─ InteractComponent修改 StateComponent
```

**优点**：
- ✅ 调用直接，性能好

**缺点**：
- ❌ PreviewActor依赖InteractComponent，耦合度高
- ❌ 违反单向数据流原则
- ❌ PreviewActor需要知道InteractComponent接口
- ❌ 职责混乱（渲染层直接调用业务层）

---

### 方案B：PreviewActor直接修改StateComponent

**描述**：PreviewActor直接修改StateComponent的Geometry

**数据流**：
```
用户拖动Gizmo
└─ PreviewActor检测拖动
   └─ 直接修改 StateComponent->Geometry
```

**优点**：
- ✅ 调用直接

**缺点**：
- ❌ 违反只读访问原则
- ❌ 绕过业务逻辑层（InteractComponent）
- ❌ 可能破坏数据一致性
- ❌ 职责严重混乱

---

### 方案C：通过回调委托通知VisualUIComponent ✅

**描述**：PreviewActor通过回调委托通知VisualUIComponent，VisualUIComponent调用InteractComponent执行操作

**数据流**：
```
用户拖动Gizmo
└─ PreviewActor内部检测拖动
   └─ 调用 OnXxxGizmoDragged()
      └─ 触发 GeometryOperationCallback
         └─ VisualUIComponent接收回调
            └─ 调用 InteractComponent 执行操作
               └─ InteractComponent 修改 StateComponent
                  └─ VisualUIComponent 读取新 Geometry
                     └─ 调用 PreviewActor 更新渲染
```

**设计**：
```cpp
// 委托定义
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGeometryOperationCallback,
    EGeometryOperationType, OperationType,
    const FGeometryOperationParams&, Parameters);

// PreviewActor存储回调
FGeometryOperationCallback GeometryOperationCallback;

// Gizmo拖动时触发
void AVoxelPreviewActor::OnMoveGizmoDragged(EAxis::Type Axis, float Delta)
{
    FGeometryOperationParams Params;
    Params.MoveDelta = FIntVector(Delta, 0, 0); // 简化示例

    if (GeometryOperationCallback.IsBound())
    {
        GeometryOperationCallback.Execute(EGeometryOperationType::MoveGEO, Params);
    }
}
```

**优点**：
- ✅ PreviewActor只读访问业务数据
- ✅ 业务逻辑由InteractComponent处理
- ✅ 数据流清晰，职责分离
- ✅ 单向数据流：PreviewActor → VisualUIComponent → InteractComponent
- ✅ 低耦合：PreviewActor不依赖具体组件
- ✅ 易于测试：可以模拟回调

**缺点**：
- ⚠️ 略微间接，多一层回调
- ⚠️ 需要定义委托和参数结构

---

### 方案D：使用事件系统

**描述**：使用虚幻引擎的事件系统（如UObject::Delegate）

**优点**：
- ✅ 标准机制

**缺点**：
- ❌ 过度设计，复杂度高
- ❌ 对于简单场景不必要

---

## 四、选择方案

### 选择方案C：通过回调委托通知VisualUIComponent

**决策理由**：

1. **职责分离**：
   - PreviewActor：检测Gizmo拖动，触发回调
   - VisualUIComponent：接收回调，协调处理
   - InteractComponent：执行业务逻辑
   - 职责清晰，不混淆

2. **单向数据流**：
   - PreviewActor → VisualUIComponent → InteractComponent
   - 无反向调用
   - 数据流清晰

3. **低耦合**：
   - PreviewActor通过委托通信，不依赖具体组件
   - VisualUIComponent实现回调，解耦良好
   - 易于维护和扩展

4. **只读原则**：
   - PreviewActor只读访问StateComponent
   - 不修改业务数据
   - 符合设计原则

5. **标准化**：
   - 使用虚幻引擎的标准委托机制
   - 符合引擎最佳实践

---

## 五、设计细节

### 5.1 回调接口设计

#### 委托定义

```cpp
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGeometryOperationCallback,
    EGeometryOperationType, OperationType,
    const FGeometryOperationParams&, Parameters);
```

#### 操作类型枚举

```cpp
enum class EGeometryOperationType : uint8
{
    MoveGEO,         // 移动整个Geometry
    ModifyCoverage,  // 修改包围盒形状
    RotateGEO,       // 旋转Geometry
    Recenter         // 重置旋转中心
};
```

#### 操作参数结构

```cpp
struct FGeometryOperationParams
{
    // MoveGEO参数
    FIntVector MoveDelta;

    // ModifyCoverage参数
    EAxis::Type CoverageAxis;
    bool bMaxSide;
    int32 CoverageDelta;

    // RotateGEO参数
    FRotator RotateDelta;

    // Recenter参数
    // 无参数
};
```

### 5.2 Gizmo回调接口

| 方法 | 参数 | 触发时机 | 操作类型 |
|------|------|---------|---------|
| **OnMoveGizmoDragged** | Axis, Delta | 拖动移动Gizmo | MoveGEO |
| **OnRotateGizmoDragged** | Axis, DeltaAngle | 拖动旋转Gizmo | RotateGEO |
| **OnFaceGizmoDragged** | Axis, bMaxSide, Delta | 拖动面控件 | ModifyCoverage |

### 5.3 VisualUIComponent实现

```cpp
// VisualUIComponent初始化PreviewActor时设置回调
void UVoxelControllerVisualUIComponent::InitializePreviewActor()
{
    if (PreviewActor)
    {
        PreviewActor->SetGeometryOperationCallback(
            FGeometryOperationCallback::CreateUObject(this, &UVoxelControllerVisualUIComponent::OnGeometryOperation)
        );
    }
}

// VisualUIComponent处理回调
void UVoxelControllerVisualUIComponent::OnGeometryOperation(
    EGeometryOperationType OperationType,
    const FGeometryOperationParams& Parameters)
{
    switch (OperationType)
    {
    case EGeometryOperationType::MoveGEO:
        if (InteractComponent)
        {
            InteractComponent->MoveGEO(Parameters.MoveDelta);
        }
        break;

    case EGeometryOperationType::ModifyCoverage:
        if (InteractComponent)
        {
            InteractComponent->ModifyCoverage(
                Parameters.CoverageAxis,
                Parameters.bMaxSide,
                Parameters.CoverageDelta);
        }
        break;

    // ... 其他操作
    }

    // 操作完成后更新渲染
    UpdatePreviewFromGeometry();
}
```

---

## 六、数据流设计

### 6.1 完整数据流

```
用户拖动Gizmo
    ↓
PreviewActor内部检测拖动
    ↓
调用对应的OnXxxGizmoDragged()
    ↓
构造操作参数
    ↓
触发GeometryOperationCallback
    ↓
VisualUIComponent.OnGeometryOperation()
    ↓
调用InteractComponent执行操作
    ↓
InteractComponent修改StateComponent.Geometry
    ↓
VisualUIComponent读取新Geometry
    ↓
调用PreviewActor更新渲染
```

### 6.2 职责划分

| 组件 | 职责 |
|------|------|
| **PreviewActor** | 检测Gizmo拖动，触发回调 |
| **VisualUIComponent** | 接收回调，协调处理，更新渲染 |
| **InteractComponent** | 执行Geometry操作 |
| **StateComponent** | 存储Geometry数据 |

---

## 七、影响分析

### 7.1 正面影响

| 影响项 | 说明 |
|--------|------|
| **职责清晰** | ✅ 渲染层、协调层、业务层分离 |
| **数据流清晰** | ✅ 单向数据流，无循环调用 |
| **低耦合** | ✅ PreviewActor不依赖具体组件 |
| **可维护性** | ✅ 代码清晰，易于理解 |
| **可测试性** | ✅ 可以模拟回调进行测试 |

### 7.2 负面影响

| 影响项 | 说明 | 缓解措施 |
|--------|------|---------|
| 略微间接 | 多一层回调 | 回调机制标准化，易于理解 |
| 需要定义结构 | 需要定义委托和参数 | 结构清晰，文档完善 |

### 7.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 回调未设置 | 低 | 中 | 初始化检查，断言保护 |
| 参数错误 | 低 | 中 | 参数验证，测试覆盖 |
| 数据流循环 | 低 | 高 | Code Review，架构约束 |

---

## 八、实施计划

### 8.1 实施步骤

1. **定义委托和结构**
   ```cpp
   DECLARE_DYNAMIC_DELEGATE_TwoParams(FGeometryOperationCallback, ...);
   enum class EGeometryOperationType ...;
   struct FGeometryOperationParams ...;
   ```

2. **PreviewActor添加回调成员**
   ```cpp
   FGeometryOperationCallback GeometryOperationCallback;
   void SetGeometryOperationCallback(FGeometryOperationCallback InCallback);
   ```

3. **实现Gizmo回调接口**
   ```cpp
   void OnMoveGizmoDragged(EAxis::Type Axis, float Delta);
   void OnRotateGizmoDragged(EAxis::Type Axis, float DeltaAngle);
   void OnFaceGizmoDragged(EAxis::Type Axis, bool bMaxSide, float Delta);
   ```

4. **VisualUIComponent实现回调**
   ```cpp
   void OnGeometryOperation(EGeometryOperationType OperationType, const FGeometryOperationParams& Parameters);
   ```

5. **初始化时设置回调**
   ```cpp
   PreviewActor->SetGeometryOperationCallback(...);
   ```

### 8.2 验证方法

| 验证项 | 方法 |
|--------|------|
| 回调触发正确 | 单元测试 |
| 参数传递正确 | 集成测试 |
| 数据流单向 | Code Review |

---

## 九、相关决策

| 相关决策 | 文档 |
|---------|------|
| PreviewActor数据访问 | `预览系统-PreviewActor数据访问-决策记录.md` |
| FVoxelGeometryDesc操作方法设计 | `编辑系统-FVoxelGeometryDesc操作方法设计-决策记录.md` |

---

## 十、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状态**：✅ 已确定
