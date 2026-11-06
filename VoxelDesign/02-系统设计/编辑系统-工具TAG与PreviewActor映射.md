# 编辑系统-工具TAG与PreviewActor映射

**文档版本**: v1.0
**创建日期**: 2026-01-29
**状态**: ✅ 已确认

---

## 一、PreviewActor预览模式枚举

```cpp
enum class EPreviewMode : uint8
{
    None              = 0,  // 无预览
    SingleVoxel       = 1,  // 单体素预览（方块网格）
    SimpleGeometry    = 2,  // 简单几何体预览（外框+内部）
    ComplexBlueprint  = 3,  // 复杂蓝图预览（外框+程序化模型）
};
```

---

## 二、工具TAG与PreviewActor映射表

### 1️⃣ 选区工具（Voxel.Tool.Selection）

| 工具TAG | 预览模式 | 显示内容 | 更新方法 | Gizmo |
|---------|---------|---------|---------|-------|
| **Tool_Selection_SingleVoxel** | `None` | 无预览（选区工具不显示预览） | - | ❌ 无 |
| **Tool_Selection_SimpleGeometry** | `SimpleGeometry` | 选区预览（Wireframe + ProceduralMesh） | `UpdateSimpleGeometryPreviewWithSelection()` | ✅ 6个面Gizmo |
| **Tool_Selection_Magic** | `None` | 魔棒选区光标提示 | - | ❌ 无 |
| **Tool_Selection_Brush** | `None` | 笔刷光标范围指示器 | - | ❌ 无 |

**说明**：
- 选区工具主要用于定义选区，不是编辑操作
- **Tool_Selection_SimpleGeometry**: 使用特殊的预览逻辑
  - **WireframeMesh**: 显示最大边界框（固定，不随鼠标框选移动）
  - **ProceduralMesh**: 显示实际选中的体素（动态，随鼠标框选移动）
  - **InnerMesh**: 隐藏（使用ProceduralMesh替代）
  - **ThicknessMesh**: 隐藏（选区工具不使用厚度）
  - 支持6个面Gizmo进行包围盒的精确调整

**选区预览数据流**：
```
StateComponent::GetCurrentEditRequest()
    ↓
VisualUIComponent::HandleBuildModeParamsUpdated()
    ↓
UVoxelSelectionTool::CalculateSelection()  // 计算实际选中的体素
    ↓
PreviewActor::UpdateSimpleGeometryPreviewWithSelection(Request, SelectedVoxels)
    ↓
WireframeMesh: 最大边界（固定）
ProceduralMesh: 实际选中的体素（动态）
```

---

### 2️⃣ 编辑工具（Voxel.Tool.Edit）

| 工具TAG | 预览模式 | 显示内容 | 更新方法 | Gizmo |
|---------|---------|---------|---------|-------|
| **Tool_Edit_SingleVoxel** | `SingleVoxel` | 单个方块实体预览 | `UpdateSingleVoxelPreview()` | ❌ 无 |
| **Tool_Edit_SimpleGeometry** | `SimpleGeometry` | 几何体预览（外框+内部） | `UpdateSimpleGeometryPreview()` | ✅ 12个Gizmo |
| **Tool_Edit_Selection** | `None` | 选区操作无预览（移动/复制选区时显示虚线框） | - | ✅ 移动Gizmo |
| **Tool_Edit_Template** | `ComplexBlueprint` | 蓝图预览（外框+程序化模型） | `UpdateComplexBlueprintPreview()` | ✅ 旋转Gizmo |
| **Tool_Edit_Furniture** | `ComplexBlueprint` | 家具预览（外框+实际模型） | `UpdateComplexBlueprintPreview()` | ✅ 旋转Gizmo |

**说明**：
- **Tool_Edit_SingleVoxel**: 放置/删除单个体素，显示实际材质和颜色
- **Tool_Edit_SimpleGeometry**: 批量建造/挖空，支持厚度、旋转、正方体模式
  - 显示12个Gizmo（6个面平移 + 3个轴向旋转 + 3个轴向比例）
  - `ThicknessMesh` 显示挖空或扩张边界
- **Tool_Edit_Selection**: 选区操作（移动、复制、删除选区内容）
  - 移动模式：显示移动Gizmo
  - 复制模式：显示源选区（虚线）+目标位置预览
  - 删除模式：仅高亮显示选区
- **Tool_Edit_Template**: 放置预定义模板（建筑、结构）
  - 外框线框 + 内部程序化生成的网格
  - 支持旋转调整
- **Tool_Edit_Furniture**: 放置家具/建造物品
  - 外框线框 + 实际家具模型
  - 支持旋转调整

---

### 3️⃣ 采样工具（Voxel.Tool.Sample）

| 工具TAG | 预览模式 | 显示内容 | 更新方法 | Gizmo |
|---------|---------|---------|---------|-------|
| **Tool_Sample** | `None` | 采样光标 | - | ❌ 无 |

**说明**：
- 采样工具只显示光标，不显示预览

---

### 4️⃣ 区域编辑工具（Voxel.Tool.AreaEdit）

| 工具TAG | 预览模式 | 显示内容 | 更新方法 | Gizmo |
|---------|---------|---------|---------|-------|
| **Tool_AreaEdit_Create** | `SimpleGeometry` | 区域范围预览（外框+内部） | `UpdateSimpleGeometryPreview()` | ✅ 6个面Gizmo |
| **Tool_AreaEdit_Modify** | `SimpleGeometry` | 区域修改预览（外框+内部） | `UpdateSimpleGeometryPreview()` | ✅ 6个面Gizmo |

**说明**：
- 区域编辑工具使用几何体预览显示区域范围
- 显示完整的包围框预览（外框线框+内部填充），与其他几何工具一致
- 支持6个面Gizmo进行区域范围的调整

---

## 三、PreviewActor组件使用规则

### 组件可见性矩阵

| 预览模式 | 工具类型 | WireframeMesh | InnerMesh | ThicknessMesh | BlueprintMesh (ProceduralMesh) |
|---------|---------|--------------|-----------|---------------|-------------------------------|
| **None** | - | ❌ 隐藏 | ❌ 隐藏 | ❌ 隐藏 | ❌ 隐藏 |
| **SingleVoxel** | 编辑工具 | ❌ 隐藏 | ✅ 显示 | ❌ 隐藏 | ❌ 隐藏 |
| **SimpleGeometry** | 编辑工具 | ✅ 显示 | ✅ 显示 | ⚠️ 条件显示 | ❌ 隐藏 |
| **SimpleGeometry** | 选区工具 | ✅ 显示 | ❌ 隐藏 | ❌ 隐藏 | ✅ 显示（实际选区体素） |
| **SimpleGeometry** | 区域编辑工具 | ✅ 显示 | ✅ 显示 | ❌ 隐藏 | ❌ 隐藏 |
| **ComplexBlueprint** | 编辑工具 | ✅ 显示 | ❌ 隐藏 | ❌ 隐藏 | ✅ 显示（蓝图网格） |

**ThicknessMesh 条件显示**（仅编辑工具）：
- `Thickness > 0`: 显示内部挖空边界（红色）
- `Thickness < 0`: 显示外部扩张边界（蓝色）
- `Thickness = 0`: 隐藏

**选区工具特殊显示**：
- **WireframeMesh**: 最大边界框（固定，基于Geometry的Bound）
- **ProceduralMesh**: 实际选中的体素（动态，基于UVoxelSelectionTool::CalculateSelection()）
- **InnerMesh**: 隐藏（使用ProceduralMesh替代）
- **ThicknessMesh**: 隐藏（选区工具不使用厚度）

---

## 四、工具切换流程

### 当前工具TAG判断逻辑（替代BatchMode）

```cpp
// 旧代码（使用BatchMode）
if (SC->IsBatchModeEnabled())
{
    UpdateBatchEditPreview(Request);
}
else
{
    UpdateSingleVoxelPreview(Position, BlockType, Color);
}

// 新代码（使用工具TAG）
FGameplayTag CurrentTool = SC->GetCurrentToolTag();

if (CurrentTool == FCoreTags::Tool_Edit_SingleVoxel)
{
    // 单体素编辑
    UpdateSingleVoxelPreview(Position, BlockType, Color);
}
else if (CurrentTool == FCoreTags::Tool_Edit_SimpleGeometry)
{
    // 几何体编辑
    UpdateSimpleGeometryPreview(Request, BlockLength);
}
else if (CurrentTool == FCoreTags::Tool_Edit_Template ||
         CurrentTool == FCoreTags::Tool_Edit_Furniture)
{
    // 模板/家具放置
    UpdateComplexBlueprintPreview(Bounds, MeshData, BlockType);
}
```

---

## 五、Gizmo显示规则

### Gizmo类型与工具映射

| Gizmo类型 | 对应工具 | 显示时机 |
|----------|---------|---------|
| **无Gizmo** | Tool_Edit_SingleVoxel, Tool_Selection_SingleVoxel, Tool_Sample | 单体素操作 |
| **6个面Gizmo** | Tool_Edit_SimpleGeometry, Tool_Selection_SimpleGeometry, Tool_AreaEdit_* | 几何体调整 |
| **12个Gizmo** | Tool_Edit_SimpleGeometry（完整模式） | 几何体完整编辑 |
| **移动Gizmo** | Tool_Edit_Selection（移动模式） | 选区移动 |
| **旋转Gizmo** | Tool_Edit_Template, Tool_Edit_Furniture | 模板/家具旋转 |

**Gizmo组合说明**：
- 6个面Gizmo：平移包围盒的6个面
- 12个Gizmo：6个面平移 + 3个轴向旋转 + 3个轴向比例
- 移动Gizmo：3个轴向箭头（X/Y/Z）
- 旋转Gizmo：3个轴向圆环（X/Y/Z）

---

## 六、实现检查清单

### 代码修改点

#### ✅ 已完成
- [x] 删除 `VoxelControllerStateComponent.h` 中的 `BatchMode` 成员
- [x] 删除 `VoxelControllerStateComponent.cpp` 中的 `SetBatchMode()` 方法
- [x] 删除 `VoxelControllerStateComponent.cpp` 中的 `IsBatchModeEnabled()` 方法
- [x] 重构 `VoxelControllerVisualUIComponent.cpp` 中的预览模式判断
- [x] 重构 `VoxelControllerInteractComponent.cpp` 中的 `SetBatchEditMode()` 方法
- [x] 重构 `VoxelControllerInteractComponent.cpp` 中的 `HandleGizmoInteraction()` 方法

#### 🔄 待完成
- [ ] 更新 `BuildPanelWidget.cpp` 中废弃的 `BatchMode` 引用
- [ ] 更新相关设计文档
- [ ] 更新架构文档
- [ ] 编写单元测试

---

## 七、关键代码示例

### 示例1: 工具TAG判断替代BatchMode

**文件**: `VoxelControllerVisualUIComponent.cpp:605`

```cpp
// 旧代码
if (!CachedStateComponent->IsBatchModeEnabled())
{
    UpdateSingleVoxelPreview(...);
}
else
{
    UpdateBatchEditPreview(...);
}

// 新代码
FGameplayTag CurrentToolTag = CachedStateComponent->GetCurrentToolTag();

if (CurrentToolTag == FCoreTags::Tool_Edit_SingleVoxel)
{
    UpdateSingleVoxelPreview(...);
}
else if (CurrentToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
{
    UpdateBatchEditPreview(...);
}
```

### 示例2: 工具TAG判断替代BatchMode（InteractComponent）

**文件**: `VoxelControllerInteractComponent.cpp:648`

```cpp
// 旧代码
if (!SC->IsBatchModeEnabled())
{
    // 单体素模式
    UpdateSingleVoxelEditPreview(...);
}
else
{
    // 批量模式
    HandleGizmoInteraction(...);
}

// 新代码
FGameplayTag CurrentTool = SC->GetCurrentToolTag();

if (CurrentTool == FCoreTags::Tool_Edit_SingleVoxel)
{
    // 单体素编辑
    UpdateSingleVoxelEditPreview(...);
}
else if (CurrentTool == FCoreTags::Tool_Edit_SimpleGeometry)
{
    // 几何体编辑
    HandleGizmoInteraction(...);
}
```

### 示例3: 删除BatchMode设置

**文件**: `VoxelControllerInteractComponent.cpp:38`

```cpp
// 旧代码
void UVoxelControllerInteractComponent::SetBatchEditMode(bool bEnable)
{
    if (UVoxelControllerStateComponent* SC = GetControllerState())
    {
        SC->SetBatchMode(bEnable);  // ❌ 删除
    }
}

// 新代码
void UVoxelControllerInteractComponent::SwitchToGeometryEditTool()
{
    if (UVoxelControllerStateComponent* SC = GetControllerState())
    {
        SC->SetCurrentToolTag(FCoreTags::Tool_Edit_SimpleGeometry);  // ✅ 切换工具
    }
}
```

### 示例4: 选区和区域编辑工具的预览处理

**文件**: `VoxelControllerVisualUIComponent.cpp:594-638`

```cpp
// 新代码（区分选区和编辑工具）
FGameplayTag CurrentToolTag = CachedStateComponent->GetBuildState();

// ==================== 1. 单体素编辑工具 ====================
if (CurrentToolTag == FCoreTags::Tool_Edit_SingleVoxel)
{
    UpdateSingleVoxelPreview(...);
}
// ==================== 2. 选区工具（使用特殊预览逻辑） ====================
else if (CurrentToolTag == FCoreTags::Tool_Selection_SimpleGeometry)
{
    if (PreviewActor)
    {
        float BlockLength = 100.0f;
        if (UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
        {
            BlockLength = GameInstance->WorldConfig.BlockLength;
        }

        // 获取编辑请求
        FVoxelEditRequest Request = CachedStateComponent->GetCurrentEditRequest();

        // 计算选区数据（实际选中的体素）
        TArray<FIntVector> SelectedVoxels = UVoxelSelectionTool::CalculateSelection(Request, BlockLength);

        // 使用选区版本的方法更新预览
        PreviewActor->UpdateSimpleGeometryPreviewWithSelection(Request, BlockLength, SelectedVoxels);
    }
}
// ==================== 3. 几何体编辑和区域编辑工具 ====================
else if (CurrentToolTag == FCoreTags::Tool_Edit_SimpleGeometry ||
         CurrentToolTag == FCoreTags::Tool_AreaEdit_Create ||
         CurrentToolTag == FCoreTags::Tool_AreaEdit_Modify)
{
    // 几何体预览：显示包围框（外框+内部）
    if (PreviewActor)
    {
        float BlockLength = 100.0f;
        if (UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
        {
            BlockLength = GameInstance->WorldConfig.BlockLength;
        }

        FVoxelEditRequest Request = CachedStateComponent->GetCurrentEditRequest();
        UpdateBatchEditPreview(Request);  // 编辑和区域工具统一使用批量预览
    }
}
// ==================== 4. 其他工具 ====================
else
{
    ClearPreview();
}
```

**说明**：
- **选区工具**：使用 `UpdateSimpleGeometryPreviewWithSelection()` 显示实际选中的体素
- **编辑/区域工具**：使用 `UpdateBatchEditPreview()` 显示几何体预览
- 选区工具的预览数据通过 `UVoxelSelectionTool::CalculateSelection()` 实时计算

---

## 八、测试验证

### 测试用例

| 测试场景 | 操作步骤 | 预期结果 |
|---------|---------|---------|
| **单体素编辑** | 切换到 `Tool_Edit_SingleVoxel`，点击放置 | 显示单个方块预览 |
| **几何体编辑** | 切换到 `Tool_Edit_SimpleGeometry`，拖拽定义包围盒 | 显示几何体预览 + 12个Gizmo |
| **模板放置** | 切换到 `Tool_Edit_Template`，选择模板 | 显示蓝图预览 + 旋转Gizmo |
| **家具放置** | 切换到 `Tool_Edit_Furniture`，选择家具 | 显示家具预览 + 旋转Gizmo |
| **选区工具（几何）** | 切换到 `Tool_Selection_SimpleGeometry`，拖拽 | **显示包围框预览（外框+内部）+ 6个面Gizmo** |
| **选区工具（魔棒）** | 切换到 `Tool_Selection_Magic`，点击体素 | 显示魔棒光标，无包围框 |
| **区域创建** | 切换到 `Tool_AreaEdit_Create`，拖拽 | **显示区域范围包围框（外框+内部）+ 6个面Gizmo** |
| **区域修改** | 切换到 `Tool_AreaEdit_Modify`，选择区域 | **显示区域范围包围框（外框+内部）+ 6个面Gizmo** |

---

## 九、相关文档

- [工具Tag替代BuildState枚举-决策记录](/VoxelDesign/05-设计决策记录/编辑系统-工具Tag替代BuildState枚举-决策记录.md)
- [编辑系统-Controller-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller-架构设计.md)
- [编辑系统-预览系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-预览系统-架构设计.md)
- [编辑系统-FVoxelGeometryDesc-系统设计](/VoxelDesign/02-系统设计/编辑系统-FVoxelGeometryDesc-系统设计.md)

---

**文档维护**:
- 创建日期: 2026-01-29
- 最后更新: 2026-01-29
- 维护者: Claude Code
