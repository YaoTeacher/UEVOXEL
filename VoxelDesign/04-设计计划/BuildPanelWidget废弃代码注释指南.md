# BuildPanelWidget 废弃代码注释指南

**目的**: 标记并注释 BuildPanelWidget 和 BuildAreaModify 中已迁移到模块系统的重复代码

**日期**: 2026-01-27

---

## 迁移原则

- ✅ **保留**: 不在模块系统中或需要全局访问的功能
- ❌ **废弃**: 已完全迁移到特定模块的功能
- ⚠️ **待定**: 需要进一步讨论是否迁移的功能

---

## BuildPanelWidget.h 需要注释的部分

### 1. 形状选择按钮组 → M3_GeometryShapeModule

**位置**: Line 123-131

**需要注释的代码**:
```cpp
// 形状选择按钮组
UPROPERTY(meta = (BindWidget))
UCheckBox* BoxShapeButton;

UPROPERTY(meta = (BindWidget))
UCheckBox* SphereShapeButton;

UPROPERTY(meta = (BindWidget))
UCheckBox* CylinderShapeButton;

UPROPERTY(meta = (BindWidget))
UCheckBox* PyramidShapeButton;
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M3_GeometryShapeModule]
// 形状选择按钮组 - 已移至 M3: UM3_GeometryShapeModule
// ============================================================
// UPROPERTY(meta = (BindWidget))
// UCheckBox* BoxShapeButton;
// ... (其他三个按钮)
```

---

### 2. 几何参数控制控件 → M4b_GeometryParamsModule

**位置**: Line 136-161

**需要注释的代码**:
```cpp
/** 旋转控制滑块 */
UPROPERTY(meta = (BindWidget))
class USlider* RotationPitchSlider;

UPROPERTY(meta = (BindWidget))
class USlider* RotationYawSlider;

UPROPERTY(meta = (BindWidget))
class USlider* RotationRollSlider;

/** 旋转数值显示 */
UPROPERTY(meta = (BindWidget))
UTextBlock* RotationPitchText;

UPROPERTY(meta = (BindWidget))
UTextBlock* RotationYawText;

UPROPERTY(meta = (BindWidget))
UTextBlock* RotationRollText;

/** 厚度控制输入框 */
UPROPERTY(meta = (BindWidget))
class UEditableTextBox* ThicknessInput;

/** 正方体模式复选框 */
UPROPERTY(meta = (BindWidget))
UCheckBox* RegularModeCheckBox;
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M4b_GeometryParamsModule]
// 几何参数控制控件 - 已移至 M4b: UM4b_GeometryParamsModule
// ============================================================
// UPROPERTY(meta = (BindWidget))
// class USlider* RotationPitchSlider;
// ... (其他控件)
```

---

### 3. 几何参数控制方法 → M4b_GeometryParamsModule

**位置**: Line 422-442

**需要注释的代码**:
```cpp
/** 旋转滑块值改变 */
UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnRotationPitchChanged(float Value);

UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnRotationYawChanged(float Value);

UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnRotationRollChanged(float Value);

/** 厚度输入框文本改变 */
UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnThicknessTextChanged(const FText& Text);

/** 正方体模式复选框状态改变 */
UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnRegularModeCheckChanged(bool bChecked);

/** 更新几何参数UI显示 */
UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void UpdateGeometryControls(const FVoxelGeometryDesc& Geometry);
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M4b_GeometryParamsModule]
// 几何参数控制方法 - 已移至 M4b: UM4b_GeometryParamsModule
// ============================================================
// UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
// void OnRotationPitchChanged(float Value);
// ... (其他方法)
```

---

### 4. 形状按钮控制方法 → M3_GeometryShapeModule

**位置**: Line 341, 398-399, 418

**需要注释的代码**:
```cpp
UFUNCTION()
void HandleShapeButtonChanged(EBatchShapeType NewShape);

UFUNCTION(BlueprintCallable, Category = "BuildPanel|Action")
void ResetShapeButton();

UFUNCTION(BlueprintCallable, Category = "BuildPanel|Action")
void UpdateShapeButton(EBatchShapeType NewShape);
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M3_GeometryShapeModule]
// 形状按钮控制 - 已移至 M3: UM3_GeometryShapeModule
// ============================================================
// UFUNCTION()
// void HandleShapeButtonChanged(EBatchShapeType NewShape);
// ... (其他方法)
```

---

## BuildPanelWidget.cpp 需要注释的部分

### 1. 形状按钮初始化和绑定

**查找位置**: 搜索 `BoxShapeButton`, `SphereShapeButton` 等初始化和事件绑定代码

**示例代码**:
```cpp
// 初始化形状按钮
if (BoxShapeButton)
{
    BoxShapeButton->OnCheckStateChanged.AddDynamic(this, &UBuildPanelWidget::OnBoxShapeChanged);
}
// ... (其他形状按钮)
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M3_GeometryShapeModule]
// 形状按钮初始化 - 已移至 M3: UM3_GeometryShapeModule
// ============================================================
// if (BoxShapeButton)
// {
//     BoxShapeButton->OnCheckStateChanged.AddDynamic(this, &UBuildPanelWidget::OnBoxShapeChanged);
// }
// ... (其他形状按钮)
```

---

### 2. 几何参数控件初始化和绑定

**查找位置**: 搜索 `RotationPitchSlider`, `ThicknessInput`, `RegularModeCheckBox` 等初始化和事件绑定代码

**示例代码**:
```cpp
// 绑定旋转滑块事件
if (RotationPitchSlider)
{
    RotationPitchSlider->OnValueChanged.AddDynamic(this, &UBuildPanelWidget::OnRotationPitchChanged);
}
// ... (其他控件)
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M4b_GeometryParamsModule]
// 几何参数控件初始化 - 已移至 M4b: UM4b_GeometryParamsModule
// ============================================================
// if (RotationPitchSlider)
// {
//     RotationPitchSlider->OnValueChanged.AddDynamic(this, &UBuildPanelWidget::OnRotationPitchChanged);
// }
// ... (其他控件)
```

---

### 3. 几何参数控制方法实现

**查找位置**: 搜索 `OnRotationPitchChanged`, `OnThicknessTextChanged`, `UpdateGeometryControls` 等方法实现

**示例代码**:
```cpp
void UBuildPanelWidget::OnRotationPitchChanged(float Value)
{
    // 实现代码...
}

void UBuildPanelWidget::UpdateGeometryControls(const FVoxelGeometryDesc& Geometry)
{
    // 实现代码...
}
```

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M4b_GeometryParamsModule]
// 几何参数控制方法实现 - 已移至 M4b: UM4b_GeometryParamsModule
// ============================================================
// void UBuildPanelWidget::OnRotationPitchChanged(float Value)
// {
//     // 原实现代码已移至 M4b: UM4b_GeometryParamsModule::OnRotationPitchChanged
// }
```

---

### 4. 形状按钮方法实现

**查找位置**: 搜索 `HandleShapeButtonChanged`, `UpdateShapeButton`, `ResetShapeButton` 等方法实现

**注释模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M3_GeometryShapeModule]
// 形状按钮方法实现 - 已移至 M3: UM3_GeometryShapeModule
// ============================================================
// void UBuildPanelWidget::HandleShapeButtonChanged(EBatchShapeType NewShape)
// {
//     // 原实现代码已移至 M3: UM3_GeometryShapeModule::OnBoxShapeChanged 等
// }
```

---

## BuildAreaModify 需要审核的部分

### 可能重复的功能

1. **区域信息显示** → M8_AreaInfoModule
2. **墙壁方向编辑** → M9_WallDirectionModule
3. **区域基础信息编辑** → M13a_AreaBasicInfoEditModule

**需要检查 BuildAreaModifyWidget.h/.cpp 中是否有这些功能的UI控件和事件处理。**

---

## 批量注释脚本

### PowerShell 脚本示例

```powershell
# 注释形状选择按钮
$file = "Source\Voxel\Public\Core\UI\Build\BuildPanelWidget.h"
$content = Get-Content $file
$content = $content -replace 'UPROPERTY\(meta = \(BindWidget\)\)\s+UCheckBox\* BoxShapeButton;', '// [已废弃 - 移至 M3] UPROPERTY(meta = (BindWidget)) // UCheckBox* BoxShapeButton;'
$content | Set-Content $file
```

### 手动注释步骤

1. 在 IDE 中打开 `BuildPanelWidget.h`
2. 使用查找功能搜索：
   - `BoxShapeButton`
   - `RotationPitchSlider`
   - `ThicknessInput`
   - `RegularModeCheckBox`
   - `OnRotationPitchChanged`
   - `HandleShapeButtonChanged`
   - `UpdateGeometryControls`
3. 逐个添加注释标记
4. 对 `BuildPanelWidget.cpp` 重复相同操作

---

## 完成检查清单

- [ ] BuildPanelWidget.h 中形状选择相关代码已注释
- [ ] BuildPanelWidget.h 中几何参数控件已注释
- [ ] BuildPanelWidget.h 中相关方法已注释
- [ ] BuildPanelWidget.cpp 中形状选择初始化已注释
- [ ] BuildPanelWidget.cpp 中几何参数控件初始化已注释
- [ ] BuildPanelWidget.cpp 中相关方法实现已注释
- [ ] 编译无错误
- [ ] 审查 BuildAreaModify 中是否有重复功能
- [ ] 更新相关文档

---

## 相关模块文档

- M3: `Source/Voxel/Public/Core/UI/Build/Modules/M3_GeometryShapeModule.h`
- M4b: `Source/Voxel/Public/Core/UI/Build/Modules/M4b_GeometryParamsModule.h`
- 模块系统设计: `VoxelDesign/03-架构设计/编辑系统-工具系统-架构设计.md`
