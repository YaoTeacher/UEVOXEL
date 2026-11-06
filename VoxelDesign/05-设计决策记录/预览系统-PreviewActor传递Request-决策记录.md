# PreviewActor传递Request决策记录

## 决策概述

**决策内容**：PreviewActor统一更新接口从传递`Geometry`改为传递`FVoxelEditRequest`

**决策日期**：2025-01

**决策者**：架构团队

**状态**：✅ 已实施

---

## 1. 问题背景

### 1.1 原有设计

在之前的架构中，PreviewActor的统一更新接口是：

```cpp
void AVoxelPreviewActor::UpdateFromGeometry(const FVoxelGeometryDesc& Geometry)
{
    // 需要查询StateComponent获取业务状态
    FRenderContext Context = GetRenderContext();  // 查询 BuildState/EditState/BlockType
    UpdateRenderFromGeometry(Geometry, Context);
    UpdateGizmosFromGeometry(Geometry);
}
```

**数据流**：
```
StateComponent.SetTmpGeometry()
  ↓
广播 OnTmpGeometryChanged(Geometry)  // 只传递Geometry
  ↓
VisualUIComponent.HandleTmpGeometryChanged(Geometry)
  ↓
PreviewActor.UpdateFromGeometry(Geometry)
  ↓
PreviewActor查询StateComponent  // ❌ 重复查询
```

### 1.2 存在的问题

#### 问题1：重复查询业务状态

**现象**：
- StateComponent已经拥有完整的`FVoxelEditRequest`（包含Geometry + BuildState + EditState + BlockType）
- 但广播只传递`Geometry`
- PreviewActor接收到Geometry后，需要再次查询StateComponent获取业务状态

**代码示例**：
```cpp
// StateComponent.cpp
void SetTmpGeometry(const FVoxelGeometryDesc& Geometry)
{
    TmpGeometry = Geometry;
    OnTmpGeometryChanged.Broadcast(TmpGeometry);  // 只传Geometry
}

// PreviewActor.cpp
FRenderContext AVoxelPreviewActor::GetRenderContext() const
{
    FRenderContext Context;
    if (AVoxelPlayerController* PC = OwningController.Get())
    {
        if (UVoxelControllerStateComponent* StateComp = PC->GetComponentByClass<UVoxelControllerStateComponent>())
        {
            Context.BuildState = StateComp->GetBuildState();      // 重复查询
            Context.EditState = StateComp->GetEditState();        // 重复查询
            Context.BlockType = StateComp->GetInteractableDataInHand().ItemTag;  // 重复查询
        }
    }
    return Context;
}
```

**影响**：
- ❌ 性能浪费：重复查询相同的组件
- ❌ 职责不清：StateComponent既提供数据，又被PreviewActor查询
- ❌ 数据冗余：VisualUIComponent和PreviewActor都需要访问StateComponent

#### 问题2：Geometry职责混乱

**现象**：
- Geometry本应是纯粹的几何数学描述（位置、大小、形状、旋转）
- 但为了满足渲染需求，需要在PreviewActor中查询并附加业务状态
- 导致"几何数据"和"业务状态"的边界模糊

**问题**：
```cpp
struct FVoxelGeometryDesc
{
    FVoxelBox Box;           // ✅ 几何数据
    EBatchShapeType ShapeType;  // ✅ 几何数据
    FRotator Rotation;       // ✅ 几何数据
    int32 Thickness;         // ✅ 几何数据
    // ❌ 缺少 BuildState/EditState/BlockType
};
```

如果将业务状态添加到Geometry中：
```cpp
struct FVoxelGeometryDesc
{
    // 几何数据
    FVoxelBox Box;
    EBatchShapeType ShapeType;
    FRotator Rotation;
    int32 Thickness;

    // ❌ 业务状态混杂
    EVoxelControllerBuildState BuildState;
    EVoxelControllerEditState EditState;
    FGameplayTag BlockType;
};
```

这会导致：
- ❌ Geometry不再纯粹是"几何描述"
- ❌ 几何计算逻辑和业务逻辑混在一起
- ❌ 违反单一职责原则

---

## 2. 候选方案

### 方案A：扩展Geometry包含状态

```cpp
struct FVoxelGeometryDesc
{
    // 几何数据
    FVoxelBox Box;
    EBatchShapeType ShapeType;
    FRotator Rotation;
    int32 Thickness;

    // 添加业务状态
    EVoxelControllerBuildState BuildState;
    EVoxelControllerEditState EditState;
    FGameplayTag BlockType;
};
```

**优点**：
- ✅ 接口简单：仍然只传递一个参数
- ✅ 一次传输包含所有信息

**缺点**：
- ❌ **Geometry不再是纯粹的几何描述**
- ❌ 混杂了数学概念和业务状态
- ❌ 违反单一职责原则
- ❌ 几何算法和业务逻辑耦合

---

### 方案B：传递FVoxelEditRequest

```cpp
struct FVoxelEditRequest
{
    FVoxelGeometryDesc Geometry;     // 几何数学描述
    EVoxelControllerBuildState BuildState;  // 业务状态
    EVoxelControllerEditState EditState;   // 业务状态
    FGameplayTag BlockType;           // 业务状态
};

// StateComponent广播
OnTmpEditRequestChanged.Broadcast(Request);

// PreviewActor接口
void UpdateFromEditRequest(const FVoxelEditRequest& Request)
{
    // 直接使用Request中的所有数据，无需查询
    UpdateRenderFromRequest(Request);
}
```

**优点**：
- ✅ **Geometry保持纯粹**：只包含几何数学描述
- ✅ **职责清晰**：Request = 完整编辑信息，Geometry = 几何描述
- ✅ **无重复查询**：所有数据从Request获取
- ✅ **数据完整**：一次传递包含所有渲染需要的信息
- ✅ **符合业务语义**：EditRequest本身就是"编辑请求"的完整表示

**缺点**：
- ⚠️ 需要修改广播和委托签名（但这是一次性的改动）

---

## 3. 决策

### 3.1 最终选择：方案B

**决策内容**：
1. ✅ StateComponent广播`OnTmpEditRequestChanged(Request)`，传递完整的FVoxelEditRequest
2. ✅ PreviewActor接口改为`UpdateFromEditRequest(const FVoxelEditRequest& Request)`
3. ✅ PreviewActor内部直接使用Request中的所有数据，不再查询StateComponent
4. ✅ Geometry保持纯粹，只包含几何数学描述

### 3.2 决策理由

| 评估维度 | 方案A（扩展Geometry） | 方案B（传递Request） | 选择 |
|---------|---------------------|-------------------|------|
| **职责清晰** | ❌ Geometry混杂业务状态 | ✅ Request完整，Geometry纯粹 | **方案B** |
| **代码简洁** | ✅ 单参数传递 | ✅ 单参数传递 | 平局 |
| **性能优化** | ❌ 仍需扩展结构体 | ✅ 无重复查询 | **方案B** |
| **可维护性** | ❌ 违反单一职责 | ✅ 职责清晰分离 | **方案B** |
| **语义准确性** | ❌ Geometry≠几何描述 | ✅ Request=编辑请求 | **方案B** |
| **修改成本** | ✅ 只修改结构体 | ⚠️ 需要修改广播签名 | **方案A** |

**关键权衡**：
- 虽然方案B需要修改广播签名（一次性成本），但带来了长期的架构清晰度和性能提升
- Geometry作为"几何描述"的核心概念必须保持纯粹，不能混杂业务状态
- FVoxelEditRequest本身就是业务逻辑中自然存在的概念，使用它更符合业务语义

---

## 4. 实施细节

### 4.1 StateComponent修改

**广播定义**：
```cpp
// VoxelControllerStateComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTmpEditRequestChanged, const FVoxelEditRequest&, NewRequest);

UPROPERTY(BlueprintAssignable, Category = "Events|Area|Geometry")
FOnTmpEditRequestChanged OnTmpEditRequestChanged;
```

**广播触发**：
```cpp
// VoxelControllerStateComponent.cpp
void SetTmpGeometry(const FVoxelGeometryDesc& Geometry)
{
    TmpGeometry = Geometry;
    OnTmpEditRequestChanged.Broadcast(GetCurrentEditRequest());  // 传递完整Request
}
```

### 4.2 VisualUIComponent修改

**委托绑定**：
```cpp
// VoxelControllerVisualUIComponent.cpp
void BindDelegates(UVoxelControllerStateComponent* SC)
{
    SC->OnTmpEditRequestChanged.AddDynamic(this, &UVoxelControllerVisualUIComponent::HandleTmpEditRequestChanged);
}
```

**Handle函数**：
```cpp
void UVoxelControllerVisualUIComponent::HandleTmpEditRequestChanged(const FVoxelEditRequest& NewRequest)
{
    if (!PreviewActor) return;

    // 直接传递Request
    PreviewActor->UpdateFromEditRequest(NewRequest);

    // 同步坐标轴箭头
    const FBox WorldBounds = NewRequest.Geometry.GetBound(BlockLength);
    UpdateAxisArrowsPosition(WorldBounds.GetCenter());
}
```

### 4.3 PreviewActor修改

**统一接口**：
```cpp
// VoxelPreviewActor.h
UFUNCTION(BlueprintCallable, Category = "Voxel|Preview")
void UpdateFromEditRequest(const FVoxelEditRequest& Request);

// VoxelPreviewActor.cpp
void AVoxelPreviewActor::UpdateFromEditRequest(const FVoxelEditRequest& Request)
{
    if (!Request.Geometry.IsValid())
    {
        ClearPreview();
        HideAllGizmos();
        return;
    }

    UpdateRenderFromRequest(Request);
    UpdateGizmosFromGeometry(Request.Geometry);
}
```

**渲染更新**：
```cpp
void AVoxelPreviewActor::UpdateRenderFromRequest(const FVoxelEditRequest& Request)
{
    // ✅ 直接使用Request中的所有数据，无需查询StateComponent
    const EVoxelControllerBuildState BuildState = Request.BuildState;
    const EVoxelControllerEditState EditState = Request.EditState;
    const FGameplayTag BlockType = Request.BlockType;
    const FVoxelGeometryDesc& Geometry = Request.Geometry;

    // 渲染逻辑...
}
```

### 4.4 删除的内容

- ❌ `FRenderContext` 结构体（不再需要）
- ❌ `GetRenderContext()` 方法（不再查询）
- ❌ `UpdateRenderFromGeometry(Geometry, Context)` 方法
- ❌ `UpdateFromGeometry(Geometry)` 公共接口

---

## 5. 影响范围

### 5.1 修改的文件

**核心组件**：
- `VoxelControllerStateComponent.h/.cpp` - 广播定义和触发
- `VoxelControllerVisualUIComponent.h/.cpp` - 委托绑定和Handle函数
- `VoxelPreviewActor.h/.cpp` - 统一更新接口和内部实现

**UI组件**：
- `BuildPanelWidget.h/.cpp` - 委托绑定和Handle函数

### 5.2 数据流变化

**之前**：
```
StateComponent
  ↓ OnTmpGeometryChanged(Geometry)
VisualUIComponent
  ↓ UpdateFromGeometry(Geometry)
PreviewActor
  ↓ 查询 StateComponent
获取 BuildState/EditState/BlockType
```

**之后**：
```
StateComponent
  ↓ OnTmpEditRequestChanged(Request)
VisualUIComponent
  ↓ UpdateFromEditRequest(Request)
PreviewActor
  ✅ 直接使用 Request.BuildState/EditState/BlockType
```

---

## 6. 验证和测试

### 6.1 编译验证

- ✅ 所有引用已更新（无未定义的OnTmpGeometryChanged）
- ✅ 所有签名已匹配（委托、Handle函数、接口）

### 6.2 功能验证

**待验证**：
- [ ] 单体素预览正常显示
- [ ] 批量编辑预览正常显示
- [ ] Gizmo位置和缩放正确更新
- [ ] 不同EditState显示正确颜色
- [ ] BuildState切换正确切换渲染模式

### 6.3 性能验证

- [ ] 无重复查询StateComponent
- [ ] 广播触发频率正常
- [ ] PreviewActor更新无延迟

---

## 7. 经验总结

### 7.1 设计原则

1. **保持概念纯粹性**：Geometry = 几何数学描述，不应混杂业务状态
2. **避免重复查询**：如果上游已经拥有完整数据，应该一次性传递
3. **符合业务语义**：FVoxelEditRequest是自然的业务概念，使用它更直观
4. **单一职责原则**：每个结构体/类应该有明确的单一职责

### 7.2 架构演进

**历史演进**：
```
v1.0: PreviewActor主动查询StateComponent
  ↓ 问题：耦合严重
v2.0: StateComponent广播Geometry，PreviewActor查询业务状态
  ↓ 问题：重复查询
v3.0: StateComponent广播完整Request，PreviewActor直接使用
  ✅ 解决：无重复查询，职责清晰
```

### 7.3 可复用模式

**模式名称**：完整数据传递模式

**适用场景**：
- 上游组件拥有完整的数据集合
- 下游组件需要该数据集合的多个子集
- 避免下游组件重复查询上游组件

**实施方法**：
1. 定义包含所有需要数据的结构体（如FVoxelEditRequest）
2. 上游组件广播/传递完整结构体
3. 下游组件直接使用，不再查询

---

## 8. 相关文档

- **架构设计**：`03-架构设计/预览系统-PreviewActor-架构设计.md`
- **数据结构定义**：`VoxelEditRequest` 结构体定义
- **StateComponent设计**：`03-架构设计/状态组件-StateComponent-架构设计.md`

---

**文档版本**：v1.0
**最后更新**：2025-01
**维护者**：架构团队
