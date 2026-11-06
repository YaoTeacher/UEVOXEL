# UMG蓝图创建指南 - 工具系统详情页模块

**日期**: 2026-01-27
**版本**: v1.0

---

## 概述

本文档提供详细的步骤指导,为工具系统详情页的8个核心模块创建UMG蓝图。每个模块都是C++类的子类,需要创建对应的蓝图并设计UI布局。

---

## 前置条件

- C++模块类已编译完成
- 虚幻编辑器已打开项目
- 了解基本的UMG操作

---

## 模块列表

| 模块 | C++类 | 蓝图名称 | 功能 |
|-----|-------|---------|------|
| M1 | UM1_BuildLibraryModule | WBP_M1_BuildLibraryModule | 建造库模块 |
| M2b1 | UM2b1_PlacementModeModule | WBP_M2b1_PlacementModeModule | 放置模式模块 |
| M2b2 | UM2b2_SelectionOpModeModule | WBP_M2b2_SelectionOpModeModule | 选区运算模式模块 |
| M3 | UM3_GeometryShapeModule | WBP_M3_GeometryShapeModule | 几何形状选择模块 |
| M4a | UM4a_BoundingBoxModule | WBP_M4a_BoundingBoxModule | 包围盒定义模块 |
| M4b | UM4b_GeometryParamsModule | WBP_M4b_GeometryParamsModule | 几何参数控制模块 |
| M5 | UM5_SelectionInfoModule | WBP_M5_SelectionInfoModule | 选区信息与操作模块 |
| M7 | UM7_BrushParamsModule | WBP_M7_BrushParamsModule | 笔刷参数控制模块 |

---

## 通用创建步骤

### Step 1: 创建蓝图类

1. 在Content Browser中导航到 `Game/Blueprints/UI/Build/Modules/` (如果没有请创建此文件夹)
2. 右键点击文件夹 → **Blueprint Class** → 选择对应的C++类
3. 命名为 `WBP_模块名` (例如 `WBP_M1_BuildLibraryModule`)
4. 双击打开蓝图编辑器

### Step 2: 设计UI布局

根据各模块的功能需求,设计并实现UI布局。重要提示:
- **控件名称必须与C++代码中的名称完全一致**
- 使用 `meta = (BindWidget)` 标记的控件必须绑定
- 控件类型必须匹配

### Step 3: 编译并保存

1. 点击工具栏的 **Compile** 按钮
2. 确保编译无错误
3. 保存蓝图

---

## 各模块详细布局设计

### M1: 建造库模块 (WBP_M1_BuildLibraryModule)

**控件绑定列表:**
- `HandItemSlot` (UItemSlotWidget*)
- `ItemLibraryContainer` (UVerticalBox*)
- `SearchTextBox` (UEditableTextBox*)
- `ItemListWrapBox` (UWrapBox*)
- `PrevPageButton` (UButton*)
- `NextPageButton` (UButton*)
- `PageNumberText` (UTextBlock*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 手持物品                             │
├─────────────────────────────────────┤
│ [HandItemSlot]                      │
│ 单价: [PriceText] 金币                │
├─────────────────────────────────────┤
│ 素材库                               │
│ 搜索: [SearchTextBox] [风格▼]       │
│ [常用] [全部] [MOD1] [MOD2]          │
│ ┌───────────────────────────────┐   │
│ │ [ItemListWrapBox]             │   │
│ │ - 动态生成物品列表              │   │
│ └───────────────────────────────┘   │
│ [◀ PrevPageButton] [1/10] [NextPageButton ▶] │
└─────────────────────────────────────┘
```

**实现要点:**
- HandItemSlot 需要复用现有的物品槽Widget
- ItemListWrapBox 使用WrapBox容器实现自适应布局
- 分页按钮在无更多页时禁用

---

### M2b1: 放置模式模块 (WBP_M2b1_PlacementModeModule)

**控件绑定列表:**
- `OverwriteModeCheckBox` (UCheckBox*)
- `FillModeCheckBox` (UCheckBox*)
- `ReplaceModeCheckBox` (UCheckBox*)
- `SpecifiedReplaceModeCheckBox` (UCheckBox*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 放置规则                             │
├─────────────────────────────────────┤
│ ○ OverwriteModeCheckBox 覆盖        │
│ ○ FillModeCheckBox 填充            │
│ ● ReplaceModeCheckBox 替换         │
│ ○ SpecifiedReplaceModeCheckBox 指定替换│
└─────────────────────────────────────┘
```

**实现要点:**
- 使用CheckBox实现单选逻辑(互斥)
- 每个CheckBox后添加图标和说明文字
- 使用水平布局放置CheckBox

---

### M2b2: 选区运算模式模块 (WBP_M2b2_SelectionOpModeModule)

**控件绑定列表:**
- `NewModeCheckBox` (UCheckBox*)
- `AddModeCheckBox` (UCheckBox*)
- `SubtractModeCheckBox` (UCheckBox*)
- `IntersectModeCheckBox` (UCheckBox*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 选区运算                             │
├─────────────────────────────────────┤
│ ● NewModeCheckBox 新选              │
│ ○ AddModeCheckBox 加选              │
│ ○ SubtractModeCheckBox 减选         │
│ ○ IntersectModeCheckBox 交选        │
└─────────────────────────────────────┘
```

**实现要点:**
- 类似M2b1的单选逻辑
- 可以考虑使用图标增强可读性
- 说明文字:新选/加选/减选/交选

---

### M3: 几何形状选择模块 (WBP_M3_GeometryShapeModule)

**控件绑定列表:**
- `BoxShapeCheckBox` (UCheckBox*)
- `SphereShapeCheckBox` (UCheckBox*)
- `CylinderShapeCheckBox` (UCheckBox*)
- `PyramidShapeCheckBox` (UCheckBox*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 形状选择                             │
├──────────────────┬──────────────────┤
│ ○ BoxShapeCheckBox│ ○ SphereShapeCheckBox│
│   [立方体图标]    │   [球体图标]     │
├──────────────────┼──────────────────┤
│ ○ CylinderShape│ ○ PyramidShapeCheckBox│
│   CheckBox       │                   │
│   [圆柱图标]     │   [金字塔图标]    │
└──────────────────┴──────────────────┘
```

**实现要点:**
- 使用2x2网格布局
- 每个CheckBox上方添加形状图标
- 图标下添加文字说明

---

### M4a: 包围盒定义模块 (WBP_M4a_BoundingBoxModule)

**控件绑定列表:**
- `MinXBox` (UEditableTextBox*)
- `MinYBox` (UEditableTextBox*)
- `MinZBox` (UEditableTextBox*)
- `MaxXBox` (UEditableTextBox*)
- `MaxYBox` (UEditableTextBox*)
- `MaxZBox` (UEditableTextBox*)
- `VolumeText` (UTextBlock*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 包围盒定义                           │
├─────────────────────────────────────┤
│ 最小点 (Min)                        │
│ X: [MinXBox]  Y: [MinYBox]  Z: [MinZBox] │
├─────────────────────────────────────┤
│ 最大点 (Max)                        │
│ X: [MaxXBox]  Y: [MaxYBox]  Z: [MaxZBox] │
├─────────────────────────────────────┤
│ 体积: [VolumeText] 体素              │
└─────────────────────────────────────┘
```

**实现要点:**
- 使用3列网格布局
- 每个输入框设置数值限制
- VolumeText 只读显示

---

### M4b: 几何参数控制模块 (WBP_M4b_GeometryParamsModule)

**控件绑定列表:**
- `RotationPitchSlider` (USlider*)
- `RotationPitchText` (UTextBlock*)
- `RotationYawSlider` (USlider*)
- `RotationYawText` (UTextBlock*)
- `RotationRollSlider` (USlider*)
- `RotationRollText` (UTextBlock*)
- `ThicknessInput` (UEditableTextBox*)
- `RegularModeCheckBox` (UCheckBox*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 几何参数                             │
├─────────────────────────────────────┤
│ 旋转 (Rotation)                     │
│ Pitch: [===Slider===] [PitchText]   │
│ Yaw:   [===Slider===] [YawText]     │
│ Roll:  [===Slider===] [RollText]    │
├─────────────────────────────────────┤
│ 厚度 (Thickness)                    │
│ [ThicknessInput]                    │
├─────────────────────────────────────┤
│ [RegularModeCheckBox] 正方体模式    │
└─────────────────────────────────────┘
```

**实现要点:**
- Slider范围: 0-360度
- TextBlock显示当前值
- ThicknessInput 限制为正整数

---

### M5: 选区信息与操作模块 (WBP_M5_SelectionInfoModule)

**控件绑定列表:**
- `SelectionStatusText` (UTextBlock*)
- `VoxelCountText` (UTextBlock*)
- `SizeText` (UTextBlock*)
- `CenterText` (UTextBlock*)
- `MinCoordText` (UTextBlock*)
- `MaxCoordText` (UTextBlock*)
- `ClearSelectionButton` (UButton*)
- `SaveAsTemplateButton` (UButton*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 选区信息与操作                       │
├─────────────────┬───────────────────┤
│ 状态: [Status]  │ 体素: [VoxelCount]│
│ 尺寸: [Size]    │                   │
│ 中心: [Center]  │                   │
├─────────────────┴───────────────────┤
│ 最小坐标: [MinCoordText]            │
│ 最大坐标: [MaxCoordText]            │
├─────────────────────────────────────┤
│ [ClearSelectionButton] [SaveAsTemplateButton] │
└─────────────────────────────────────┘
```

**实现要点:**
- 使用2列网格布局显示信息
- 无选区时显示 "-"
- 无选区时禁用按钮

---

### M7: 笔刷参数控制模块 (WBP_M7_BrushParamsModule)

**控件绑定列表:**
- `CircleBrushCheckBox` (UCheckBox*)
- `SquareBrushCheckBox` (UCheckBox*)
- `BrushSizeSlider` (USlider*)
- `DrawModeCheckBox` (UCheckBox*)
- `ClickModeCheckBox` (UCheckBox*)

**推荐布局:**
```
┌─────────────────────────────────────┐
│ 笔刷参数                             │
├─────────────────────────────────────┤
│ 形状                                 │
│ ● CircleBrushCheckBox 圆形          │
│ ○ SquareBrushCheckBox 方形          │
├─────────────────────────────────────┤
│ 大小: [===BrushSizeSlider===] [50]  │
├─────────────────────────────────────┤
│ 模式                                 │
│ ● DrawModeCheckBox 绘制             │
│ ○ ClickModeCheckBox 点击            │
└─────────────────────────────────────┘
```

**实现要点:**
- 形状和模式分别使用单选逻辑
- BrushSizeSlider 范围: 1-100
- 可以添加实时数值显示

---

## 常见问题

### Q: 控件绑定失败?

A: 检查以下几点:
1. 控件名称与C++代码完全一致(区分大小写)
2. 控件类型匹配
3. 蓝图编译成功
4. 使用 `meta = (BindWidget)` 标记

### Q: 模块不显示?

A: 检查以下几点:
1. 蓝图类已添加到 BuildPanelWidget 的模块配置中
2. `ShouldShowForTool()` 返回 true
3. 当前工具标签匹配
4. ToolDetailPanel 容器存在

### Q: 事件不响应?

A: 检查以下几点:
1. 事件在 `NativeConstruct()` 中绑定
2. 使用 `UFUNCTION()` 宏标记
3. 使用 `AddDynamic` 绑定委托
4. 蓝图编译成功

---

## 调试技巧

### 1. 查看模块注册日志

在PIE模式下,查看Output Log中的模块注册信息:
```
LogTemp: BuildPanel: Modules initialized (8 modules)
LogTemp: BuildPanel: Module registered: WBP_M1_BuildLibraryModule
...
```

### 2. 检查模块可见性

添加调试日志到 `UpdateModuleVisibility()`:
```cpp
UE_LOG(LogTemp, Warning, TEXT("Module %s visibility: %s"),
    *Module->GetName(),
    bShouldShow ? TEXT("Visible") : TEXT("Collapsed"));
```

### 3. 验证控件绑定

在 `NativeConstruct()` 中添加检查:
```cpp
if (!SomeWidget)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to bind widget: SomeWidget"));
}
```

---

## 完成检查清单

- ☐ 所有8个模块蓝图创建完成
- ☐ 所有控件正确绑定
- ☐ 所有蓝图编译成功
- ☐ UI布局符合设计
- ☐ 在编辑器中测试模块显示
- ☐ 验证模块功能正常

---

## 后续步骤

完成蓝图创建后:
1. 更新 BuildPanelWidget 配置蓝图类
2. 启动PIE测试工具切换
3. 验证模块显示逻辑
4. 测试模块功能

---

## 相关文档

- [实施计划](/VoxelDesign/04-设计计划/2026-01-27-工具系统详情页实施计划.md)
- [系统设计](/VoxelDesign/02-系统设计/编辑系统-工具系统-系统设计.md)
- [架构设计](/VoxelDesign/03-架构设计/编辑系统-工具系统-架构设计.md)
