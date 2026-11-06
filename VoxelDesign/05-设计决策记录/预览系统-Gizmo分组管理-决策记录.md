# 预览系统 Gizmo分组管理 决策记录

> **决策日期**：2026-01-16
> **决策状态**：✅ 已确定
> **相关文档**：`预览系统-Gizmo管理-系统设计.md`

---

## 一、问题背景

PreviewActor需要管理12个Gizmo控件（6个Actor控件 + 6个面控件）。关键问题是：如何组织和管理这些Gizmo？

**核心问题**：Gizmo应该如何分组管理？

**问题重要性**：
- 关系到Gizmo显示控制
- 影响代码组织
- 关系到Gizmo交互逻辑

---

## 二、现状分析

### 2.1 Gizmo分类

**Actor控件组（6个）**：
- 移动Gizmo：3个（X/Y/Z轴箭头）
- 旋转Gizmo：3个（X/Y/Z轴圆环）

**面控件组（6个）**：
- 前面/后面/左面/右面/下面/上面

### 2.2 显示需求

不同场景下需要显示不同的Gizmo组合：
- **单体素编辑**：不显示任何Gizmo
- **几何体编辑**：显示所有Gizmo（6个Actor + 6个面）
- **蓝图编辑**：只显示Actor控件（6个）

---

## 三、候选方案

### 方案A：所有Gizmo统一管理

**描述**：所有12个Gizmo放在一个数组中统一管理

**设计**：
```cpp
TArray<TObjectPtr<AVoxel3DUIActor>> AllGizmos; // 12个
```

**优点**：
- ✅ 管理简单，一个数组

**缺点**：
- ❌ 无法独立控制显示/隐藏
- ❌ 不同Gizmo有不同的交互逻辑，处理复杂
- ❌ 更新逻辑混乱（Actor控件基于Center，面控件基于包围盒）
- ❌ 显示控制复杂

---

### 方案B：按类型分组（移动/旋转/面）

**描述**：按功能类型分成三组管理

**设计**：
```cpp
TArray<TObjectPtr<AVoxel3DUIActor>> MoveGizmos;    // 3个
TArray<TObjectPtr<AVoxel3DUIActor>> RotateGizmos;  // 3个
TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;    // 6个
```

**优点**：
- ✅ 可以按功能独立控制

**缺点**：
- ❌ 移动和旋转都是Actor控件，应该一起管理
- ❌ 分组不够清晰

---

### 方案C：按控件分组（Actor控件组 + 面控件组）✅

**描述**：按照控件性质分成两组：Actor控件组和面控件组

**设计**：
```cpp
// Actor控件组（6个：移动3 + 旋转3）
TArray<TObjectPtr<AVoxel3DUIActor>> ActorGizmos;

// 面控件组（6个）
TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;
```

**分组理由**：
- **Actor控件组**：移动Gizmo和旋转Gizmo
  - 都是操作Actor整体的控件
  - 位置都基于Center
  - 在相同场景下显示/隐藏

- **面控件组**：6个面控件
  - 都是调整包围盒形状的控件
  - 位置基于包围盒的6个面
  - 在相同场景下显示/隐藏

**优点**：
- ✅ 职责清晰，便于独立控制
- ✅ 不同的交互逻辑分开处理
- ✅ 不同的更新规则（Actor控件基于Center，面控件基于包围盒）
- ✅ 显示控制简单（双重控制矩阵）
- ✅ 符合物理分类

**缺点**：
- 无明显缺点

---

## 四、选择方案

### 选择方案C：按控件分组（Actor控件组 + 面控件组）

**决策理由**：

1. **职责清晰**：
   - Actor控件：操作Actor整体（移动、旋转）
   - 面控件：调整包围盒形状
   - 职责明确，易于理解

2. **显示控制简单**：
   - 根据双重控制矩阵（编辑类型 × 流程状态）决定显示哪些组
   - 几何体编辑：两组都显示
   - 蓝图编辑：只显示Actor控件组
   - 单体素编辑：两组都不显示

3. **更新逻辑独立**：
   - Actor控件：位置基于Center
   - 面控件：位置基于包围盒的6个面
   - 更新方法独立，逻辑清晰

4. **交互逻辑独立**：
   - Actor控件：触发MoveGEO和RotateGEO
   - 面控件：触发ModifyCoverage
   - 逻辑不混淆

5. **易于扩展**：
   - 未来添加新的Actor控件（如缩放）只需加到Actor控件组
   - 未来添加新的面控件（如对角线拖动）只需加到面控件组

---

## 五、显示控制矩阵

### 5.1 双重控制矩阵

| 编辑类型 | 流程状态 | Actor控件 | 面控件 | 说明 |
|---------|---------|----------|--------|------|
| EditConstruction | EDIT | ❌ | ❌ | 单体素不需要Gizmo |
| EditArea | EDIT | ✅ | ✅ | 几何体显示所有Gizmo |
| PlaceFurniture | EDIT | ✅ | ❌ | 蓝图只显示Actor控件 |
| 任意 | HIDDEN/BASE | ❌ | ❌ | 隐藏所有 |

### 5.2 显示控制实现

```cpp
void AVoxelPreviewActor::UpdateGizmoVisibility()
{
    bool bShowActorGizmos = false;
    bool bShowFaceGizmos = false;

    if (PreviewState == EPreviewInteractionState::EDIT)
    {
        switch (CurrentMode)
        {
        case EPreviewMode::SimpleGeometry:
            bShowActorGizmos = true;
            bShowFaceGizmos = true;
            break;
        case EPreviewMode::ComplexBlueprint:
            bShowActorGizmos = true;
            bShowFaceGizmos = false;
            break;
        case EPreviewMode::SingleVoxel:
            bShowActorGizmos = false;
            bShowFaceGizmos = false;
            break;
        }
    }

    // 更新Actor控件组
    for (AVoxel3DUIActor* Gizmo : ActorGizmos)
    {
        Gizmo->SetActorHiddenInGame(!bShowActorGizmos);
        Gizmo->SetActorEnableCollision(bShowActorGizmos);
    }

    // 更新面控件组
    for (AVoxel3DUIActor* Gizmo : FaceGizmos)
    {
        Gizmo->SetActorHiddenInGame(!bShowFaceGizmos);
        Gizmo->SetActorEnableCollision(bShowFaceGizmos);
    }
}
```

---

## 六、更新逻辑设计

### 6.1 Actor控件位置更新

```cpp
void AVoxelPreviewActor::UpdateActorGizmosPosition(FVector Center)
{
    // 所有Actor控件都基于Center定位
    for (AVoxel3DUIActor* Gizmo : ActorGizmos)
    {
        if (Gizmo)
        {
            Gizmo->SetActorLocation(Center);
        }
    }
}
```

### 6.2 面控件位置更新

```cpp
void AVoxelPreviewActor::UpdateFaceGizmosPosition(const FBox& Bounds)
{
    FVector Centers[6];
    Centers[0] = FVector(Bounds.Min.X, Bounds.GetCenter().Y, Bounds.GetCenter().Z);  // 前面
    Centers[1] = FVector(Bounds.Max.X, Bounds.GetCenter().Y, Bounds.GetCenter().Z);  // 后面
    Centers[2] = FVector(Bounds.GetCenter().X, Bounds.Min.Y, Bounds.GetCenter().Z);  // 左面
    Centers[3] = FVector(Bounds.GetCenter().X, Bounds.Max.Y, Bounds.GetCenter().Z);  // 右面
    Centers[4] = FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);  // 下面
    Centers[5] = FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z);  // 上面

    for (int32 i = 0; i < 6; i++)
    {
        if (FaceGizmos[i])
        {
            FaceGizmos[i]->SetActorLocation(Centers[i]);
        }
    }
}
```

---

## 七、影响分析

### 7.1 正面影响

| 影响项 | 说明 |
|--------|------|
| **职责清晰** | ✅ Actor控件和面控件职责明确 |
| **显示控制简单** | ✅ 基于分组控制，逻辑清晰 |
| **更新逻辑独立** | ✅ 两组更新逻辑独立，不混淆 |
| **交互逻辑独立** | ✅ 触发不同操作，逻辑清晰 |
| **易于扩展** | ✅ 新增控件只需加入对应组 |

### 7.2 负面影响

| 影响项 | 说明 | 缓解措施 |
|--------|------|---------|
| 无明显负面影响 | - | - |

### 7.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 分组错误 | 低 | 低 | 文档清晰，代码注释 |
| 显示控制错误 | 低 | 中 | 测试覆盖，Code Review |

---

## 八、实施计划

### 8.1 实施步骤

1. **定义分组**
   ```cpp
   UPROPERTY(BlueprintReadOnly, Category = "Preview|Gizmo")
   TArray<TObjectPtr<AVoxel3DUIActor>> ActorGizmos;  // 6个

   UPROPERTY(BlueprintReadOnly, Category = "Preview|Gizmo")
   TArray<TObjectPtr<AVoxel3DUIActor>> FaceGizmos;  // 6个
   ```

2. **初始化Gizmo**
   ```cpp
   void InitActorGizmos();  // 创建6个Actor控件
   void InitFaceGizmos();   // 创建6个面控件
   ```

3. **实现显示控制**
   ```cpp
   void UpdateGizmoVisibility();
   ```

4. **实现位置更新**
   ```cpp
   void UpdateActorGizmosPosition(FVector Center);
   void UpdateFaceGizmosPosition(const FBox& Bounds);
   ```

### 8.2 验证方法

| 验证项 | 方法 |
|--------|------|
| 显示控制正确性 | 单元测试 |
| 位置更新正确性 | 集成测试 |
| 交互触发正确 | 手动测试 |

---

## 九、相关决策

| 相关决策 | 文档 |
|---------|------|
| PreviewActor状态管理 | `预览系统-PreviewActor状态管理-决策记录.md` |
| Geometry操作回调机制 | `预览系统-Geometry操作回调机制-决策记录.md` |

---

## 十、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状态**：✅ 已确定
