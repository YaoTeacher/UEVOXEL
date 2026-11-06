# BuildPanelWidget 蓝图配置指南

**目的**: 配置BuildPanelWidget使用UMG蓝图类创建工具详情页模块

**适用场景**: 完成UMG蓝图创建后,在编辑器中配置蓝图类引用

---

## 配置步骤

### Step 1: 打开BuildPanelWidget蓝图

1. 在Content Browser中找到 `WBP_BuildPanelWidget` (BuildPanelWidget的蓝图)
2. 双击打开蓝图编辑器

### Step 2: 配置模块蓝图类

在蓝图编辑器的 **Details** 面板中:

1. 找到 **Category: UI | ToolDetailModules** 分类
2. 为每个模块配置对应的蓝图类:

| 属性名称 | 蓝图类路径 |
|---------|-----------|
| M1_BuildLibraryModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M1_BuildLibraryModule.WBP_M1_BuildLibraryModule_C` |
| M2b1_PlacementModeModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M2b1_PlacementModeModule.WBP_M2b1_PlacementModeModule_C` |
| M2b2_SelectionOpModeModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M2b2_SelectionOpModeModule.WBP_M2b2_SelectionOpModeModule_C` |
| M3_GeometryShapeModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M3_GeometryShapeModule.WBP_M3_GeometryShapeModule_C` |
| M4a_BoundingBoxModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M4a_BoundingBoxModule.WBP_M4a_BoundingBoxModule_C` |
| M4b_GeometryParamsModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M4b_GeometryParamsModule.WBP_M4b_GeometryParamsModule_C` |
| M5_SelectionInfoModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M5_SelectionInfoModule.WBP_M5_SelectionInfoModule_C` |
| M7_BrushParamsModuleClass | `/Game/Blueprints/UI/Build/Modules/WBP_M7_BrushParamsModule.WBP_M7_BrushParamsModule_C` |

**配置方法:**
1. 点击属性右侧的下拉箭头
2. 在弹出的类选择器中搜索对应的蓝图类
3. 选择蓝图类(后缀为 `_C`)

### Step 3: 编译并保存

1. 点击工具栏的 **Compile** 按钮
2. 确保编译无错误
3. 保存蓝图

---

## 验证配置

### 方法1: 查看Output Log

启动PIE模式后,查看Output Log:

```
LogTemp: BuildPanel: Modules initialized (8 modules)
LogTemp: BuildPanel: Module registered: WBP_M1_BuildLibraryModule_C
LogTemp: BuildPanel: Module registered: WBP_M2b1_PlacementModeModule_C
...
```

如果看到蓝图类名称(带`_C`后缀),说明配置成功。

### 方法2: 检查模块显示

1. 启动PIE模式
2. 切换到不同工具
3. 验证对应模块是否正确显示

---

## 注意事项

### 蓝图类未配置时

如果某个模块的蓝图类未配置,系统会自动使用C++类创建模块,不会报错。这允许渐进式开发:
- 可以先配置部分模块的蓝图
- 其他模块继续使用C++类
- 后续再补充蓝图类

### 蓝图类引用丢失

如果蓝图被重命名或移动,引用会丢失:
- 重新配置对应的蓝图类
- 或者在C++中更新路径

### 编译错误

如果蓝图编译失败:
1. 检查控件绑定是否正确
2. 确保所有`meta = (BindWidget)`控件已绑定
3. 查看Output Log中的详细错误信息

---

## 常见问题

### Q: 配置后模块不显示?

A: 检查以下几点:
1. 蓝图编译成功
2. 控件绑定正确
3. 当前工具标签匹配
4. `ShouldShowForTool()` 返回 true

### Q: 蓝图类无法在下拉列表中找到?

A: 可能原因:
1. 蓝图未编译
2. 蓝图类名不匹配
3. 蓝图不在Content Browser搜索路径中

解决方法:
- 编译蓝图
- 确认蓝图继承自正确的C++类
- 刷新Content Browser

### Q: 如何快速清空所有配置?

A: 在Details面板中:
1. 右键点击 **UI | ToolDetailModules** 分类
2. 选择 **Reset to Default**
3. 所有配置恢复默认(nullptr),系统会使用C++类

---

## 调试技巧

### 添加调试日志

在`BuildPanelWidget.cpp`的`InitializeModules()`中添加日志:

```cpp
UE_LOG(LogTemp, Warning, TEXT("M1 Blueprint Class: %s"),
    M1_BuildLibraryModuleClass ? *M1_BuildLibraryModuleClass->GetName() : TEXT("nullptr (using C++ class)"));
```

### 使用Visual Debugger

在PIE模式下:
1. 按 `Shift+F1` 打开鼠标光标
2. 右键点击模块 → **Debug** → **Start Debugging**
3. 查看模块属性和状态

---

## 完成检查清单

- ☐ 所有8个模块蓝图创建完成
- ☐ BuildPanelWidget配置完成
- ☐ 蓝图编译成功
- ☐ PIE模式下验证模块显示
- ☐ 模块功能正常工作

---

## 后续步骤

完成配置后:
1. 执行完整的工具切换测试
2. 验证所有模块的功能
3. 填写测试检查清单

---

## 相关文档

- [UMG蓝图创建指南](/VoxelDesign/04-设计计划/UMG蓝图创建指南.md)
- [实施计划 Task 8](/VoxelDesign/04-设计计划/2026-01-27-工具系统详情页实施计划.md#task-8)
- [系统设计](/VoxelDesign/02-系统设计/编辑系统-工具系统-系统设计.md)
