# BuildPanelWidget 重复代码精确修改清单

**说明**: 本文档提供所有需要注释的代码的精确行号和替换内容，便于使用IDE的批量查找替换功能

---

## 头文件修改: BuildPanelWidget.h

### 修改 1: 注释剩余几何参数控件

**精确位置**:
- Line 142: `UPROPERTY(meta = (BindWidget))`
- Line 143: `class USlider* RotationYawSlider;`
- Line 145: `UPROPERTY(meta = (BindWidget))`
- Line 146: `class USlider* RotationRollSlider;`

**查找内容**:
```cpp
UPROPERTY(meta = (BindWidget))
class USlider* RotationYawSlider;
```

**替换为**:
```cpp
// [已废弃 - 移至 M4b_GeometryParamsModule]
// UPROPERTY(meta = (BindWidget))
// class USlider* RotationYawSlider;
```

**对以下内容重复相同操作**:
- `RotationRollSlider` (Line 145-146)
- `RotationPitchText` (Line 149-150)
- `RotationYawText` (Line 152-153)
- `RotationRollText` (Line 155-156)
- `ThicknessInput` (Line 159-160)
- `RegularModeCheckBox` (Line 163-164)

---

### 修改 2: 注释几何参数方法声明

**位置**: Line 424-442

**查找**:
```cpp
/** 旋转滑块值改变 */
UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
void OnRotationPitchChanged(float Value);
```

**替换为**:
```cpp
// [已废弃 - 移至 M4b_GeometryParamsModule]
// /** 旋转滑块值改变 */
// UFUNCTION(BlueprintCallable, Category = "BuildPanel|Geometry")
// void OnRotationPitchChanged(float Value);
```

**需要注释的方法**:
1. `OnRotationPitchChanged` (Line 424)
2. `OnRotationYawChanged` (Line 427)
3. `OnRotationRollChanged` (Line 430)
4. `OnThicknessTextChanged` (Line 434)
5. `OnRegularModeCheckChanged` (Line 438)
6. `UpdateGeometryControls` (Line 442)

---

### 修改 3: 注释形状按钮方法

**位置**: Line 341, 398-399, 418

**需要注释的方法**:
1. `HandleShapeButtonChanged` (Line 341)
2. `ResetShapeButton` (Line 398-399)
3. `UpdateShapeButton` (Line 418)

**查找示例**:
```cpp
UFUNCTION()
void HandleShapeButtonChanged(EBatchShapeType NewShape);
```

**替换为**:
```cpp
// [已废弃 - 移至 M3_GeometryShapeModule]
// UFUNCTION()
// void HandleShapeButtonChanged(EBatchShapeType NewShape);
```

---

## 实现文件修改: BuildPanelWidget.cpp

### 修改 4: 注释形状按钮初始化

**搜索关键字**: `BoxShapeButton`, `SphereShapeButton`, `CylinderShapeButton`, `PyramidShapeButton`

**查找位置**: 在 `NativeConstruct()` 或初始化相关方法中

**查找示例**:
```cpp
// 初始化形状按钮
if (BoxShapeButton)
{
    BoxShapeButton->OnCheckStateChanged.AddDynamic(this, &UBuildPanelWidget::OnBoxShapeChanged);
}
```

**替换为**:
```cpp
// [已废弃 - 移至 M3_GeometryShapeModule]
// // 初始化形状按钮
// if (BoxShapeButton)
// {
//     BoxShapeButton->OnCheckStateChanged.AddDynamic(this, &UBuildPanelWidget::OnBoxShapeChanged);
// }
```

---

### 修改 5: 注释几何参数控件初始化

**搜索关键字**: `RotationPitchSlider`, `RotationYawSlider`, `RotationRollSlider`, `ThicknessInput`, `RegularModeCheckBox`

**查找位置**: 在 `NativeConstruct()` 或初始化相关方法中

**查找示例**:
```cpp
// 绑定旋转滑块事件
if (RotationPitchSlider)
{
    RotationPitchSlider->OnValueChanged.AddDynamic(this, &UBuildPanelWidget::OnRotationPitchChanged);
}
```

**替换为**:
```cpp
// [已废弃 - 移至 M4b_GeometryParamsModule]
// // 绑定旋转滑块事件
// if (RotationPitchSlider)
// {
//     RotationPitchSlider->OnValueChanged.AddDynamic(this, &UBuildPanelWidget::OnRotationPitchChanged);
// }
```

---

### 修改 6: 注释几何参数方法实现

**搜索方法名**:
- `OnRotationPitchChanged`
- `OnRotationYawChanged`
- `OnRotationRollChanged`
- `OnThicknessTextChanged`
- `OnRegularModeCheckChanged`
- `UpdateGeometryControls`

**查找方法**:
```cpp
void UBuildPanelWidget::OnRotationPitchChanged(float Value)
{
    // 方法实现
}
```

**替换为**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M4b_GeometryParamsModule]
// void UBuildPanelWidget::OnRotationPitchChanged(float Value)
// {
//     // 原实现代码已移至 M4b: UM4b_GeometryParamsModule::OnRotationPitchChanged
//     // 如需查看原实现，请查看 git history 或联系开发人员
// }
```

---

### 修改 7: 注释形状按钮方法实现

**搜索方法名**:
- `HandleShapeButtonChanged`
- `UpdateShapeButton`
- `ResetShapeButton`

**替换模板**:
```cpp
// ============================================================
// [已废弃 - 功能移动到 M3_GeometryShapeModule]
// void UBuildPanelWidget::HandleShapeButtonChanged(EBatchShapeType NewShape)
// {
//     // 原实现代码已移至 M3: UM3_GeometryShapeModule
//     // 如需查看原实现，请查看 git history 或联系开发人员
// }
```

---

## VS Code / IDE 批量查找替换操作

### 操作步骤

1. **打开 BuildPanelWidget.h**

2. **查找替换形状按钮** (4次操作):
   - 查找: `SphereShapeButton`
   - 替换: `// [已废弃 - 移至 M3] SphereShapeButton`

   - 查找: `CylinderShapeButton`
   - 替换: `// [已废弃 - 移至 M3] CylinderShapeButton`

   - 查找: `PyramidShapeButton`
   - 替换: `// [已废弃 - 移至 M3] PyramidShapeButton`

3. **查找替换几何控件** (逐个进行):
   - 查找: `RotationYawSlider`
   - 替换: `// [已废弃 - 移至 M4b] RotationYawSlider`

   - 查找: `RotationRollSlider`
   - 替换: `// [已废弃 - 移至 M4b] RotationRollSlider`

   - 查找: `RotationPitchText`
   - 替换: `// [已废弃 - 移至 M4b] RotationPitchText`

   查找: `RotationYawText`
   - 替换: `// [已废弃 - 移至 M4b] RotationYawText`

   查找: `RotationRollText`
   - 替换: `// [已废弃 - 移至 M4b] RotationRollText`

   查找: `ThicknessInput`
   - 替换: `// [已废弃 - �4b] ThicknessInput`

   查找: `RegularModeCheckBox`
   - 替换: `// [已废弃 - 移至 M4b] RegularModeCheckBox`

4. **注释UPROPERTY行**:
   - 使用正则表达式查找: `^(\s*)UPROPERTY\(meta = \(BindWidget\)\)$`
   - 在匹配行上方添加: `[已废弃 - 移至对应模块]`
   - 注释整行

5. **打开 BuildPanelWidget.cpp 并重复类似操作**

---

## 完成后检查

- [ ] 编译无错误
- [ ] 运行时无警告
- [ ] 所有模块正常显示
- [ ] 确认原有功能在新模块中正常工作

---

## 备注

1. **备份**: 在开始修改前，请备份 `BuildPanelWidget.h` 和 `BuildPanelWidget.cpp`

2. **Git**: 提交前请使用 `git diff` 查看所有修改

3. **测试**: 修改后需要重新编译并测试工具切换功能

4. **保留**: 本指南文档保留，作为代码迁移的历史记录

---

**相关文件**:
- 迁移指南: `VoxelDesign/04-设计计划/BuildPanelWidget废弃代码注释指南.md`
- M3 模块: `Source/Voxel/Public/Core/UI/Build/Modules/M3_GeometryShapeModule.h`
- M4b 模块: `Source/Voxel/Public/Core/UI/Build/Modules/M4b_GeometryParamsModule.h`
