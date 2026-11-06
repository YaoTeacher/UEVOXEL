# 预览系统 PreviewActor数据访问 决策记录

> **决策日期**：2026-01-16
> **决策状态**：✅ 已确定
> **相关文档**：`预览系统-PreviewActor-架构设计.md`

---

## 一、问题背景

PreviewActor需要访问StateComponent中存储的Geometry数据来渲染预览。关键问题是：PreviewActor应该如何访问这些业务数据？

**核心问题**：PreviewActor如何访问StateComponent的Geometry数据？

**问题重要性**：
- 关系到数据流的清晰性
- 影响职责分离
- 关系到数据一致性

---

## 二、需求分析

### 2.1 功能需求

1. **读取Geometry数据**：PreviewActor需要读取Geometry用于渲染
2. **实时性**：数据应该是实时的，反映最新状态
3. **单向数据流**：PreviewActor不应该修改业务数据

### 2.2 约束条件

- PreviewActor是渲染层，不应该处理业务逻辑
- StateComponent是业务数据，需要保护
- 必须避免循环依赖

### 2.3 质量属性

- **职责清晰**：渲染层和业务层分离
- **数据一致性**：保证数据实时和一致
- **可维护性**：代码清晰，易于维护

---

## 三、候选方案

### 方案A：VisualUIComponent传递数据副本

**描述**：VisualUIComponent读取StateComponent数据，传递副本给PreviewActor

**数据流**：
```
StateComponent → VisualUIComponent读取 → 复制数据 → 传递给PreviewActor
```

**优点**：
- ✅ PreviewActor完全独立，无依赖
- ✅ 数据隔离，安全

**缺点**：
- ❌ 数据复制开销
- ❌ 数据可能不同步
- ❌ 需要每次手动传递
- ❌ 数据一致性差

---

### 方案B：PreviewActor存储Geometry副本

**描述**：PreviewActor内部存储Geometry副本，VisualUIComponent更新时同步

**数据流**：
```
VisualUIComponent → 更新PreviewActor.Geometry副本 → PreviewActor使用副本
```

**优点**：
- ✅ PreviewActor有本地数据，访问快速
- ✅ 无需实时读取

**缺点**：
- ❌ 数据冗余
- ❌ 同步复杂，容易不一致
- ❌ 内存开销
- ❌ 违反单一数据源原则

---

### 方案C：PreviewActor存储Controller指针，实时读取 ✅

**描述**：PreviewActor存储Controller指针（弱引用），通过指针实时读取StateComponent

**数据流**：
```
PreviewActor → OwningController → StateComponent → Geometry数据
```

**优点**：
- ✅ 数据实时，无同步问题
- ✅ 单一数据源，无冗余
- ✅ 数据一致性保证
- ✅ 符合单向数据流原则

**缺点**：
- ⚠️ PreviewActor依赖Controller和StateComponent
- ⚠️ 使用弱引用，需要检查有效性

**实现示例**：
```cpp
// PreviewActor存储弱引用
TWeakObjectPtr<AVoxelPlayerController> OwningController;

// 读取时实时获取
FVoxelGeometryDesc AVoxelPreviewActor::GetGeometryFromState() const
{
    if (OwningController.IsValid())
    {
        if (UVoxelControllerStateComponent* State = OwningController->FindComponentByState<UVoxelControllerStateComponent>())
        {
            return State->GetTmpGeometry();
        }
    }
    return FVoxelGeometryDesc(); // 返回默认值
}
```

---

### 方案D：PreviewActor直接存储StateComponent指针

**描述**：PreviewActor直接存储StateComponent指针

**优点**：
- ✅ 访问直接

**缺点**：
- ❌ 绕过Controller，层次混乱
- ❌ PreviewActor不应该知道StateComponent存在
- ❌ 违反封装原则

---

## 四、选择方案

### 选择方案C：PreviewActor存储Controller指针，实时读取

**决策理由**：

1. **数据实时性**：
   - 实时读取，总是最新数据
   - 无同步问题，无延迟

2. **单一数据源**：
   - 避免数据冗余
   - 数据一致性保证
   - 符合DRY原则

3. **单向数据流**：
   - PreviewActor只读StateComponent
   - 不写入业务数据
   - 职责清晰

4. **架构清晰**：
   - 通过Controller访问，层次清晰
   - PreviewActor → Controller → StateComponent
   - 符合分层架构

5. **性能考虑**：
   - 指针访问开销小
   - 弱引用避免循环引用
   - 无数据复制开销

---

## 五、设计细节

### 5.1 使用弱引用的原因

| 原因 | 说明 |
|------|------|
| **避免循环引用** | PreviewActor被Controller持有，如果强引用Controller会造成循环 |
| **生命周期安全** | Controller销毁时，弱引用自动失效 |
| **有效性检查** | 使用IsValid()检查指针有效性 |

### 5.2 访问路径

```
AVoxelPreviewActor
└─ OwningController（TWeakObjectPtr<AVoxelPlayerController>）
   └─ UVoxelControllerStateComponent（通过FindComponentByState获取）
      └─ GetTmpGeometry() → FVoxelGeometryDesc
```

### 5.3 使用原则

```
✅ 允许：
├─ 读取StateComponent的Geometry数据
└─ 使用数据计算渲染位置和形状

❌ 禁止：
├─ 修改StateComponent的数据
├─ 直接调用Geometry的操作方法
└─ 写入任何业务数据
```

---

## 六、影响分析

### 6.1 正面影响

| 影响项 | 说明 |
|--------|------|
| **数据实时性** | ✅ 无同步延迟，总是最新 |
| **数据一致性** | ✅ 单一数据源，保证一致 |
| **架构清晰** | ✅ 分层明确，职责清晰 |
| **性能** | ✅ 无复制开销，性能好 |

### 6.2 负面影响

| 影响项 | 说明 | 缓解措施 |
|--------|------|---------|
| 依赖关系 | PreviewActor依赖Controller | 使用弱引用，避免强耦合 |
| 有效性检查 | 需要检查指针有效性 | 封装在GetGeometryFromState中 |

### 6.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| Controller无效 | 低 | 中 | 弱引用+有效性检查 |
| 误写数据 | 低 | 高 | 只读访问，代码审查 |
| 性能问题 | 低 | 低 | 指针访问，开销极小 |

---

## 七、实施计划

### 7.1 实施步骤

1. **添加弱引用成员**
   ```cpp
   UPROPERTY()
   TWeakObjectPtr<AVoxelPlayerController> OwningController;
   ```

2. **实现SetOwningController**
   ```cpp
   void SetOwningController(AVoxelPlayerController* Controller);
   ```

3. **实现GetGeometryFromState**
   ```cpp
   FVoxelGeometryDesc GetGeometryFromState() const;
   ```

4. **初始化时设置Controller**
   - VisualUIComponent创建PreviewActor后
   - 立即调用SetOwningController

### 7.2 验证方法

| 验证项 | 方法 |
|--------|------|
| 弱引用有效性 | 单元测试 |
| 只读访问 | 静态分析+代码审查 |
| 数据实时性 | 集成测试 |

---

## 八、相关决策

| 相关决策 | 文档 |
|---------|------|
| PreviewActor状态管理 | `预览系统-PreviewActor状态管理-决策记录.md` |
| Geometry操作回调机制 | `预览系统-Geometry操作回调机制-决策记录.md` |

---

## 九、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状态**：✅ 已确定
